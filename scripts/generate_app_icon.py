#!/usr/bin/env python3
"""Generate the original IMPlatform vector icon and portable PNG/ICNS; stdlib only."""
from pathlib import Path
import math,struct,zlib
ROOT=Path(__file__).resolve().parents[1]/'client/resources'
def rounded(x,y,box,r):
 a,b,c,d=box
 if not(a<=x<=c and b<=y<=d):return False
 dx=max(a+r-x,0,x-(c-r));dy=max(b+r-y,0,y-(d-r))
 return dx*dx+dy*dy<=r*r
def triangle(x,y,p):
 signs=[(x-p[i][0])*(p[(i+1)%3][1]-p[i][1])-(y-p[i][1])*(p[(i+1)%3][0]-p[i][0]) for i in range(3)]
 return all(s>=0 for s in signs) or all(s<=0 for s in signs)
def color(x,y):
 c=(0,0,0,0)
 if rounded(x,y,(48,48,976,976),210):c=(233,239,247,255)
 if rounded(x,y,(168,210,730,570),84) or triangle(x,y,[(244,530),(244,644),(370,550)]):c=(255,255,255,255)
 if rounded(x,y,(334,418,856,766),84) or triangle(x,y,[(680,724),(802,820),(802,704)]):c=(34,100,176,255)
 if any((x-a)**2+(y-590)**2<24**2 for a in (468,596,724)):c=(255,255,255,255)
 return c
def png(size):
 raw=bytearray()
 for y in range(size):
  raw.append(0)
  for x in range(size):
   if size<=128:
    samples=[color((x+dx)*1024/size,(y+dy)*1024/size) for dx,dy in ((.25,.25),(.75,.25),(.25,.75),(.75,.75))]
    raw.extend(sum(c[i] for c in samples)//4 for i in range(4))
   else:raw.extend(color((x+.5)*1024/size,(y+.5)*1024/size))
 def chunk(t,b):return struct.pack('!I',len(b))+t+b+struct.pack('!I',zlib.crc32(t+b)&0xffffffff)
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',size,size,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
def main():
 ROOT.mkdir(parents=True,exist_ok=True)
 svg='''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024"><rect x="48" y="48" width="928" height="928" rx="210" fill="#e9eff7"/><path fill="white" d="M252 210h394q84 0 84 84v192q0 84-84 84H370l-126 74V558q-76-4-76-72V294q0-84 84-84Z"/><path fill="#2264b0" d="M418 418h354q84 0 84 84v180q0 74-54 84v54l-122-54H418q-84 0-84-84V502q0-84 84-84Z"/><g fill="white"><circle cx="468" cy="590" r="24"/><circle cx="596" cy="590" r="24"/><circle cx="724" cy="590" r="24"/></g></svg>'''
 (ROOT/'app-icon.svg').write_text(svg+'\n')
 records=[]
 for kind,size in [(b'icp4',16),(b'icp5',32),(b'icp6',64),(b'ic07',128),(b'ic08',256),(b'ic09',512),(b'ic10',1024)]:
  data=png(size);records.append(kind+struct.pack('!I',len(data)+8)+data)
  if size==1024:(ROOT/'app-icon.png').write_bytes(data)
 body=b''.join(records);(ROOT/'IMPlatform.icns').write_bytes(b'icns'+struct.pack('!I',len(body)+8)+body)
 print('Generated original SVG, PNG and7-sizeICNS in client/resources')
if __name__=='__main__':main()
