import {createRequire} from 'node:module';
import {writeFile} from 'node:fs/promises';
const require=createRequire(process.env.JLINUX_PLAYWRIGHT_MODULE+'/package.json');
const {chromium}=require('playwright');
const [url,html,css,script]=process.argv.slice(2);
const browser=await chromium.launch({headless:true});
try{
  const page=await browser.newPage();
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto(url);await page.locator('h1').filter({hasText:'original'}).waitFor();
  await writeFile(html,'<link rel="stylesheet" href="style.css"><h1>updated</h1><script src="app.js"></script>');
  await page.waitForFunction(()=>document.querySelector('h1').textContent==='updated');
  await writeFile(css,'h1{color:rgb(0,0,255)}');
  await page.waitForFunction(()=>getComputedStyle(document.querySelector('h1')).color==='rgb(0, 0, 255)');
  await writeFile(script,'document.body.dataset.live="updated"');
  await page.waitForFunction(()=>document.body.dataset.live==='updated');
  if(errors.length)throw new Error(errors.join('\n'));
  console.log('Browser: HTML, CSS and JavaScript reloaded automatically.');
}finally{await browser.close();}
