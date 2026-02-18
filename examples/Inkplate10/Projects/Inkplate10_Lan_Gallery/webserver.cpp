#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// Functions from main .ino file
void startFileUpload(const char* filename);
void writeFileData(uint8_t* data, size_t len);
void finishFileUpload();

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Inkplate LAN Gallery</title>
<style>
body{font-family:"Segoe UI",sans-serif;background:#f3f3f3;display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;margin:0}
.container{background:#fff;padding:2rem 3rem;border-radius:12px;box-shadow:0 3px 12px rgba(0,0,0,.15);text-align:center;max-width:420px}
input[type=file]{margin:20px 0}
input[type=submit]{background:#0078d7;border:none;color:#fff;padding:10px 22px;border-radius:6px;cursor:pointer;font-size:1rem}
input[type=submit]:hover{background:#005fa3}
#status{margin-top:15px;font-size:.9rem;color:#555}
footer{position:absolute;bottom:10px;font-size:.8rem;color:#999}
</style></head><body>
<div class="container">
<h2>Inkplate LAN Gallery</h2>
<p>Upload an image to the shared gallery.</p>
<form id="uploadForm">
<input type="file" id="fileInput" accept="image/*"><br>
<input type="submit" value="Upload">
</form>
<p id="status"></p>
</div>
<footer>Connected to <b>langallery.local</b></footer>
<script>
const f=document.getElementById('uploadForm'),i=document.getElementById('fileInput'),s=document.getElementById('status');
f.addEventListener('submit',async e=>{
 e.preventDefault();const file=i.files[0];if(!file)return;
 s.textContent='Processing image...';
 const img=new Image();img.src=URL.createObjectURL(file);await img.decode();
 const maxW=1200,maxH=825;let w=img.width,h=img.height;
 if(w>maxW||h>maxH){const r=Math.min(maxW/w,maxH/h);w=Math.floor(w*r);h=Math.floor(h*r);s.textContent='Resizing image...';}
 const c=document.createElement('canvas');c.width=w;c.height=h;const x=c.getContext('2d');x.drawImage(img,0,0,w,h);
 const blob=await new Promise(r=>c.toBlob(r,'image/jpeg',0.85));
 const name=file.name.replace(/\.[^.]+$/,'.jpg');
 const up=new File([blob],name,{type:'image/jpeg'});
 URL.revokeObjectURL(img.src);
 const fd=new FormData();fd.append('file',up,name);s.textContent='Uploading...';
 try{const r=await fetch('/upload',{method:'POST',body:fd});const t=await r.text();s.textContent=t.includes('OK')?'Upload complete ✅':t;}catch(err){s.textContent='Error: '+err;}
});
</script></body></html>
)rawliteral";

void setupWebServer(){
 if(MDNS.begin("langallery"))Serial.println("mDNS: http://langallery.local/");

 server.on("/",HTTP_GET,[](AsyncWebServerRequest*r){r->send_P(200,"text/html",INDEX_HTML);});

 server.on(
  "/upload",HTTP_POST,
  [](AsyncWebServerRequest *r){r->send(200,"text/plain","OK");},
  [](AsyncWebServerRequest *r,String fn,size_t index,uint8_t*data,size_t len,bool final){
   if(!index){
     String path="/"+fn;
     Serial.printf("Starting upload: %s\n",fn.c_str());
     startFileUpload(path.c_str());
   }
   writeFileData(data, len);
   if(final){
     finishFileUpload();
   }
  }
 );

 server.begin();
}