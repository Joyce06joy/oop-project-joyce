#!/usr/bin/env python3
"""Redact machine paths and test fixture bodies. Pass/fail, IDs and counters stay unchanged."""
from pathlib import Path
import argparse,re
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
root=str(Path(__file__).resolve().parents[1]);home=str(Path.home())
text=args.source.read_text(errors='replace').replace(root,'$PROJECT_ROOT').replace(home,'$HOME')
text=re.sub(r'content= [^\n]*','content= <test message redacted>',text)
text=re.sub(r'login= "[^"]*"','login= <test account>',text)
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(text)
