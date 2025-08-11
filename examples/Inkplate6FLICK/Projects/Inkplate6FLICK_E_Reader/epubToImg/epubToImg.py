#!/usr/bin/env python3
import argparse
import tempfile
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path
from io import BytesIO
from PIL import Image
from playwright.sync_api import sync_playwright

class EPUBSmartSplitter:
    def __init__(self, epub_path: Path, out_dir: Path, width: int, height: int, threshold: int, text_size: str):
        self.epub_path = epub_path
        self.out_dir = out_dir
        self.width = width
        self.base_height = height
        self.threshold = threshold
        self.text_size = text_size
        self.out_dir.mkdir(exist_ok=True, parents=True)

    def extract_spine_htmls(self, workdir: Path) -> list[Path]:
        with zipfile.ZipFile(self.epub_path, "r") as z:
            z.extractall(workdir)

        container_xml = workdir / "META-INF" / "container.xml"
        if not container_xml.exists():
            raise FileNotFoundError("EPUB container.xml not found")

        ns = {"ct": "urn:oasis:names:tc:opendocument:xmlns:container"}
        rootfile = ET.parse(container_xml).find("ct:rootfiles/ct:rootfile", ns)
        if rootfile is None:
            raise RuntimeError("No rootfile in container.xml")
        
        opf_path = workdir / rootfile.get("full-path")
        package = ET.parse(opf_path).getroot()
        ns = {"opf": package.tag.split('}')[0].strip('{')}

        manifest = {item.get("id"): item.get("href") 
                   for item in package.findall("opf:manifest/opf:item", ns)}
        
        spine_items = [it.get("idref") 
                      for it in package.findall("opf:spine/opf:itemref", ns)
                      if it.get("linear", "yes") != "no"]
        
        htmls = []
        for item_id in spine_items:
            if item_id in manifest:
                html_path = (opf_path.parent / manifest[item_id]).resolve()
                if html_path.exists():
                    htmls.append(html_path)
        
        return htmls

    def is_text_cut_off(self, image: Image.Image) -> bool:
        """Check if the bottom row of the image contains non-white pixels"""
        bottom_row = image.crop((0, image.height-1, image.width, image.height))
        return any(pixel < self.threshold for pixel in bottom_row.getdata())

    def find_clean_break(self, page, scroller, current_top: int, max_height: int) -> int:
        """Find the maximum height where no text is cut off"""
        min_height = int(self.base_height * 0.3)  # Minimum acceptable height
        current_height = min(max_height, self.base_height)
        
        best_height = current_height
        for test_height in range(current_height, min_height - 1, -1):
            page.set_viewport_size({"width": self.width, "height": test_height})
            scroller.evaluate("(el, top) => el.scrollTop = top", current_top)
            page.wait_for_timeout(50)  # Small delay for rendering
            
            # Capture and check the bottom
            screenshot = Image.open(BytesIO(page.screenshot())).convert("L")
            if not self.is_text_cut_off(screenshot):
                best_height = test_height
                break
        
        return best_height

    def process_html(self, page, html_path: Path, start_idx: int) -> int:
        page.goto(f"file://{html_path.as_posix()}")
        page.wait_for_load_state("networkidle")

        # Set base URL for relative resources
        page.evaluate(f"""() => {{
            const base = document.createElement('base');
            base.href = 'file://{html_path.parent.as_posix()}/';
            document.head.prepend(base);
        }}""")

        # Inject CSS to control text size
        page.evaluate(f"""textSize => {{
            const style = document.createElement('style');
            style.textContent = `body {{ font-size: ${{textSize}} !important; }}`;
            document.head.appendChild(style);
        }}""", self.text_size)

        scroller = page.evaluate_handle("""() => {
            return document.scrollingElement || document.documentElement || document.body;
        }""")

        total_height = scroller.evaluate("(el) => el.scrollHeight")
        current_top = 0
        idx = start_idx

        while current_top < total_height:
            remaining_height = total_height - current_top  # Fixed: Calculate remaining height
            viewport_height = self.find_clean_break(
                page, scroller, current_top, remaining_height
            )

            # Set final viewport size and position
            page.set_viewport_size({"width": self.width, "height": viewport_height})
            scroller.evaluate("(el, top) => el.scrollTop = top", current_top)
            page.wait_for_timeout(100)

            # Capture and save
            screenshot = Image.open(BytesIO(page.screenshot())).convert("L")
            bw_image = screenshot.point(lambda p: 255 if p > self.threshold else 0, mode="1")
            
            out_path = self.out_dir / f"{idx:04d}.png"
            bw_image.save(out_path, optimize=True)
            print(f"Saved {out_path.name} (height: {viewport_height}px)")
            
            current_top += viewport_height
            idx += 1

        return idx

    def process(self):
        with tempfile.TemporaryDirectory() as tmpdir, sync_playwright() as playwright:
            browser = playwright.chromium.launch()
            context = browser.new_context(
                viewport={"width": self.width, "height": self.base_height},
                device_scale_factor=1
            )
            page = context.new_page()

            workdir = Path(tmpdir)
            html_files = self.extract_spine_htmls(workdir)
            print(f"Found {len(html_files)} HTML files in EPUB spine")

            idx = 0
            for html_file in html_files:
                print(f"Processing {html_file.name}")
                idx = self.process_html(page, html_file, idx)

            browser.close()

        print(f"Done. Created {idx} PNG files in {self.out_dir}")

def main():
    parser = argparse.ArgumentParser(
        description="EPUB to PNG with smart page breaks and adjustable text size"
    )
    parser.add_argument("epub", type=Path, help="Input EPUB file")
    parser.add_argument("outdir", type=Path, help="Output directory")
    parser.add_argument("--width", type=int, default=758, help="Viewport width")
    parser.add_argument("--height", type=int, default=940, help="Base viewport height")
    parser.add_argument("--threshold", type=int, default=220, 
                      help="White threshold (0-255)")
    parser.add_argument("--text-size", type=str, default="16px",
                      help="Font size to enforce (e.g., '12px', '1.2em')")
    args = parser.parse_args()

    if args.outdir.exists() and any(args.outdir.iterdir()):
        parser.error("Output directory must be empty")

    splitter = EPUBSmartSplitter(
        args.epub,
        args.outdir,
        args.width,
        args.height,
        args.threshold,
        args.text_size
    )
    splitter.process()

if __name__ == "__main__":
    main()