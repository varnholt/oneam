// A small original sample, drawn with browser canvas; no remote content or libraries.
globalThis.oneamSample = async () => {
  const pages = [];
  for (let page = 0; page < 3; page++) {
    const canvas = document.createElement('canvas'); canvas.width = 1000; canvas.height = 1400;
    const c = canvas.getContext('2d');
    const rect = (x, y, w, h, color, radius = 0) => {
      c.fillStyle = color; c.beginPath(); c.roundRect(x, y, w, h, radius); c.fill();
    };
    const text = (value, x, y, size, color = '#202a36') => {
      c.font = `${size}px Noto, sans-serif`; c.fillStyle = color; c.fillText(value, x, y);
    };
    const moon = (x, y, r) => {
      c.fillStyle = '#f4d99a'; c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.fill();
      c.fillStyle = '#26394d'; c.beginPath(); c.arc(x + r * .4, y - r * .25, r * .88, 0, Math.PI * 2); c.fill();
    };
    const clock = (x, y, radius) => {
      c.fillStyle = '#f7f3e8'; c.strokeStyle = '#89bba8'; c.lineWidth = 9;
      c.beginPath(); c.arc(x, y, radius, 0, Math.PI * 2); c.fill(); c.stroke();
      c.strokeStyle = '#26394d'; c.lineCap = 'round'; c.beginPath(); c.moveTo(x, y-radius*.72); c.lineTo(x,y);
      c.lineTo(x+radius*.3,y-radius*.52); c.stroke();
    };
    const book = (x, y, w) => {
      rect(x,y,w,110,'#89bba8',8); rect(x+12,y+10,w/2-16,86,'#f7f3e8',4); rect(x+w/2+4,y+10,w/2-16,86,'#f7f3e8',4);
    };
    const cat = (x, y, scale = 1) => {
      c.save(); c.translate(x,y); c.scale(scale,scale); c.fillStyle='#c78457';
      c.beginPath(); c.ellipse(0,20,86,45,0,0,Math.PI*2); c.fill();
      c.beginPath(); c.moveTo(-65,-8); c.lineTo(-60,-65); c.lineTo(-24,-38); c.lineTo(15,-42); c.lineTo(45,-65);
      c.lineTo(55,-6); c.closePath(); c.fill();
      c.strokeStyle='#573e36'; c.lineWidth=5; c.lineCap='round';
      c.beginPath(); c.moveTo(-42,-13); c.lineTo(-24,-9); c.moveTo(10,-9); c.lineTo(28,-13); c.stroke(); c.restore();
    };
    rect(0,0,1000,1400,'#f7f3e8');
    if (page === 0) {
      text('NIGHT SHIFT',80,165,88); text('A very short comic.',84,230,30,'#667267');
      rect(60,295,880,920,'#26394d',18); moon(730,470,92);
      for(let i=0;i<26;i++) rect(100+(i*137)%780,340+(i*61)%430,4,4,'#f4d99a',2);
      rect(170,850,660,365,'#172735'); rect(320,910,360,305,'#e1b76f');
      rect(340,930,320,285,'#374b50'); book(390,1095,220); cat(500,1085,.7);
      text('oneam',80,1320,38,'#667267'); text('01',850,1320,25,'#667267');
    } else if (page === 1) {
      text('JUST ONE MORE PAGE',60,105,42);
      const panels=[[60,150],[515,150],[60,720],[515,720]];
      const captions=['The house is quiet.','Someone has other plans.','An excellent bookmark.','Tomorrow can wait.'];
      panels.forEach(([x,y],i)=>{
        rect(x,y,425,520,'#dbe7db',12); rect(x+12,y+12,401,390,'#26394d',6);
        if(i===0) clock(x+212,y+198,120);
        if(i===1) { moon(x+310,y+110,42); cat(x+210,y+290,1.35); }
        if(i===2) { book(x+42,y+266,340); cat(x+218,y+270,1.35); }
        if(i===3) { moon(x+215,y+176,95); clock(x+325,y+310,43); }
        text(captions[i],x+20,y+455,23);
      });
      text('02',850,1340,25,'#667267');
    } else {
      text('YOUR TURN',80,180,80); text('A little room for your next story.',84,248,28,'#667267');
      rect(60,320,880,520,'#26394d',18); book(230,600,540); cat(510,568,2); moon(790,450,56);
      text('Open a comic from your device.',80,955,36);
      text('Use the arrows or tap a page edge to turn.',80,1030,26,'#667267');
      text('Tap the center to hide the controls.',80,1080,26,'#667267');
      text('The menu has spreads, manga mode and more.',80,1130,26,'#667267');
      text('Your files stay with you.',80,1290,28,'#667267'); text('03',850,1340,25,'#667267');
    }
    pages.push({ name: `page${page+1}.png`, data: new Uint8Array(await (await new Promise(resolve=>canvas.toBlob(resolve,'image/png'))).arrayBuffer()) });
  }
  // Store-only ZIP: the PNGs are already compressed. Standard CRC32 and headers.
  const parts=[], directory=[]; let offset=0;
  const table=new Uint32Array(256);
  for(let n=0;n<256;n++){let v=n;for(let k=0;k<8;k++)v=(v>>>1)^((v&1)?0xedb88320:0);table[n]=v;}
  const crc=data=>{let v=0xffffffff;for(const byte of data)v=(v>>>8)^table[(v^byte)&255];return(v^0xffffffff)>>>0;};
  for(const {name,data} of pages){
    const encoded=new TextEncoder().encode(name), sum=crc(data);
    const local=new Uint8Array(30+encoded.length), l=new DataView(local.buffer);
    l.setUint32(0,0x04034b50,true);l.setUint16(4,20,true);l.setUint32(14,sum,true);l.setUint32(18,data.length,true);
    l.setUint32(22,data.length,true);l.setUint16(26,encoded.length,true);local.set(encoded,30);
    const central=new Uint8Array(46+encoded.length), d=new DataView(central.buffer);
    d.setUint32(0,0x02014b50,true);d.setUint16(4,20,true);d.setUint16(6,20,true);d.setUint32(16,sum,true);
    d.setUint32(20,data.length,true);d.setUint32(24,data.length,true);d.setUint16(28,encoded.length,true);
    d.setUint32(42,offset,true);central.set(encoded,46);directory.push(central);parts.push(local,data);offset+=local.length+data.length;
  }
  const size=directory.reduce((n,p)=>n+p.length,0), end=new Uint8Array(22), e=new DataView(end.buffer);
  e.setUint32(0,0x06054b50,true);e.setUint16(8,pages.length,true);e.setUint16(10,pages.length,true);
  e.setUint32(12,size,true);e.setUint32(16,offset,true);
  return new File([...parts,...directory,end],'Night shift.cbz',{type:'application/zip',lastModified:1000});
};
