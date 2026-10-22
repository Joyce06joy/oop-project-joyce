#!/usr/bin/env python3
"""Stage/deploy IMClient on macOS without modifying build or user data."""
import argparse,json,os,plistlib,shutil,subprocess,sys
from pathlib import Path

def run(args):
 print('+',' '.join(map(str,args)),flush=True)
 return subprocess.run(list(map(str,args)),check=True,capture_output=False)
def output(args):return subprocess.check_output(list(map(str,args)),text=True)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--build',type=Path,default=Path('build/mvp5'));parser.add_argument('--qt-root',type=Path,required=True);parser.add_argument('--output',type=Path,default=Path('deliverables/MVP5-macOS-arm64'));args=parser.parse_args()
 if sys.platform!='darwin':raise SystemExit('macOS packaging requires an actual Mac; no package was generated.')
 src=args.build.resolve()/'IMClient.app';dest=args.output.resolve();tool=args.qt_root.resolve()/'bin/macdeployqt'
 if not src.is_dir() or not tool.is_file():raise SystemExit('Build IMClient and provide an installed Qt root containing bin/macdeployqt.')
 if dest.exists():raise SystemExit('Output exists; choose a new output directory. Existing files were not replaced.')
 dest.mkdir(parents=True);app=dest/'IMClient.app';shutil.copytree(src,app,symlinks=True)
 run([tool,app,'-always-overwrite','-verbose=2','-no-codesign'])
 # macdeployqt copies Qt and its dependencies. Ensure direct non-Qt json-c is local too.
 exe=app/'Contents/MacOS/IMClient';frameworks=app/'Contents/Frameworks';frameworks.mkdir(exist_ok=True)
 for line in output(['otool','-L',exe]).splitlines()[1:]:
  if not line.startswith('\t'):continue # Universal Mach-O section headers are not dependencies.
  dep=line.strip().split(' (')[0]
  if dep.startswith(('/opt/homebrew/','/usr/local/')):
   target=frameworks/Path(dep).name
   if not target.exists():shutil.copy2(Path(dep).resolve(),target)
   run(['install_name_tool','-id','@rpath/'+target.name,target]);run(['install_name_tool','-change',dep,'@executable_path/../Frameworks/'+target.name,exe])
 # Normalize deployment-tool install IDs/references that embed the staging directory.
 machos=[p for p in app.rglob('*') if p.is_file() and not p.is_symlink() and 'Mach-O' in output(['file','-b',p])]
 for p in machos:
  if p.is_relative_to(frameworks):run(['install_name_tool','-id','@rpath/'+str(p.relative_to(frameworks)),p])
  for line in output(['otool','-L',p]).splitlines()[1:]:
   if not line.startswith('\t'):continue # Universal Mach-O section headers are not dependencies.
   dep=line.strip().split(' (')[0];dependency=Path(dep)
   if dependency.is_absolute() and dependency.is_relative_to(frameworks):
    run(['install_name_tool','-change',dep,'@rpath/'+str(dependency.relative_to(frameworks)),p])
 # Inspect every Mach-O, including plugins; developer absolute dependency is a failure.
 inspected=[]
 for p in machos:
  deps=output(['otool','-L',p]);inspected.append({'file':str(p.relative_to(app)),'dependencies':[x.strip() for x in deps.splitlines() if x.startswith('\t')]})
  for line in deps.splitlines()[1:]:
   if not line.startswith('\t'):continue # Universal Mach-O section headers are not dependencies.
   dep=line.strip().split(' (')[0]
   if dep.startswith('/') and not dep.startswith(('/System/Library/','/usr/lib/')):raise RuntimeError('Unbundled dependency: '+str(p.relative_to(app))+' -> '+dep)
   if dep.startswith('@rpath/') and not (frameworks/dep[len('@rpath/'):]).exists():raise RuntimeError('Missing bundled rpath dependency: '+dep)
 # Ad-hoc signature only; no certificate, hardened runtime or notarization claim.
 run(['codesign','--force','--deep','--sign','-',app]);run(['codesign','--verify','--deep','--strict',app])
 plist=plistlib.loads((app/'Contents/Info.plist').read_bytes())
 assert (app/'Contents/PlugIns/platforms/libqcocoa.dylib').exists(),'Missing Cocoa platform plugin'
 assert (app/'Contents/Resources'/plist['CFBundleIconFile']).exists(),'Missing application icon'
 (dest/'dependency-audit.json').write_text(json.dumps({'architecture':output(['lipo','-archs',exe]).strip(),'bundle_version':plist.get('CFBundleShortVersionString'),'minimum_macos':plist.get('LSMinimumSystemVersion'),'signature':'ad-hoc; not notarized','mach_o':inspected},ensure_ascii=False,indent=2))
 server=args.build.resolve()/'IMServer'
 if server.exists():shutil.copy2(server,dest/'IMServer')
 (dest/'README.txt').write_text('IMPlatform MVP5 macOS local teaching build.\nIMClient.app contains deployed Qt/plugins/json-c.\nStart a separate IMServer first (loopback default8888); server retains developer dependencies unless separately bundled.\nThis package is ad-hoc signed, not notarized. Tested only on the documented local Mac; no universal compatibility claim.\nSee repository docs/MACOS_PACKAGING.md and docs/DEMO_MVP5.md.\n')
 print('PACKAGE_DEPENDENCIES_VERIFIED',len(inspected),'Mach-O files; architecture',output(['lipo','-archs',exe]).strip(),flush=True)
if __name__=='__main__':main()
