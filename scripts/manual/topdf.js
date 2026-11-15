const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage();
await p.goto('file://'+process.argv[2]);
await p.pdf({path:process.argv[3],format:'A4',printBackground:true,displayHeaderFooter:true,headerTemplate:'<span></span>',footerTemplate:'<div style="font-size:8px;width:100%;text-align:center;color:#777">Serge for VCV Rack · <span class="pageNumber"></span> / <span class="totalPages"></span></div>',margin:{top:'16mm',bottom:'16mm',left:'15mm',right:'15mm'}});
await b.close();})();
