#include "AppInstallerService.h"
#include <string.h>

void AppInstallerService::progress(uint8_t percent,const char *phase){
 if(!progressFn)return;
 if(percent==lastProgress&&percent!=0&&percent!=100)return;
 lastProgress=percent;
 progressFn(percent,phase,progressUser);
}

static String fileName(const String &s) { int p = s.lastIndexOf('/'); return p < 0 ? s : s.substring(p + 1); }
static bool safeId(const String &id){if(!id.length()||id.length()>24)return false;for(unsigned i=0;i<id.length();i++){char c=id[i];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;}return true;}
String AppInstallerService::installedPath(const String &id) const {return String(StoragePaths::APPS_INSTALLED)+"/"+id;}

// Never recurse into user-supplied directories; an installed package consists
// of precisely three installer-owned files. No delete outside Installed.
// Refuse to recursively delete or traverse foreign content, including when
// removing incomplete staging folders. Uninstall always leaves app data alone.
bool AppInstallerService::onlyKnownFiles(const String &dir) const {
 if (!card || !card->mounted()) return false;
 File folder=card->fs().open(dir,FILE_READ);
 if (!folder || !folder.isDirectory()) { if(folder)folder.close();return false; }
 File item=folder.openNextFile();
 bool safe=true;
 while(item){
   String name=fileName(String(item.name()));
   if(item.isDirectory() || (name!="manifest.ini"&&name!="icon.rgb565"&&
      name!="payload.txt"&&name!="receipt.bin")){safe=false;item.close();break;}
   item.close(); item=folder.openNextFile();
 }
 folder.close();return safe;
}

bool AppInstallerService::cleanKnownFiles(const String &dir) {
 if(!onlyKnownFiles(dir))return false;
 fs::FS &fs=card->fs();
 const char *files[]={"manifest.ini","icon.rgb565","payload.txt","receipt.bin"};
 for(const char *name:files){String path=dir+"/"+name;
   if(fs.exists(path)&&!fs.remove(path))return false;
 }
 return fs.rmdir(dir);
}

bool AppInstallerService::scanPackage(File &f,Qeapp::Header &h,Qeapp::Meta &m,String &error,bool verify){
 if(!f||f.isDirectory()){error="Package not found";return false;}
 if(f.size()<Qeapp::HEADER_BYTES||f.size()>Qeapp::HEADER_BYTES+Qeapp::MAX_MANIFEST+Qeapp::ICON_BYTES+Qeapp::MAX_PAYLOAD+Qeapp::SIGNATURE_BYTES){error="Package size invalid";return false;}
 if(!f.seek(0)){error="Package not seekable";return false;}
 uint8_t hdr[Qeapp::HEADER_BYTES];
 if(f.read(hdr,sizeof hdr)!=(int)sizeof hdr){error="Truncated header";return false;}
 const char *reason="";
 if(!Qeapp::parseHeader(hdr,f.size(),h,reason)){error=reason;return false;}
 char manifest[Qeapp::MAX_MANIFEST+1];
 if(f.read(reinterpret_cast<uint8_t*>(manifest),h.manifestLen)!=(int)h.manifestLen){error="Truncated manifest";return false;}
 manifest[h.manifestLen]=0;
 Qeapp::Sha256 signedBytes;signedBytes.update(hdr,sizeof hdr);
 signedBytes.update(reinterpret_cast<const uint8_t*>(manifest),h.manifestLen);
 Qeapp::Sha256 sha;sha.update(reinterpret_cast<const uint8_t*>(manifest),h.manifestLen);uint8_t digest[32];sha.finish(digest);
 if(!Qeapp::equalHash(digest,h.manifestHash)){error="Manifest SHA-256 mismatch";return false;}
 if(!Qeapp::parseManifest(manifest,h.manifestLen,m,reason)){error=reason;return false;}
 m.hasIcon=(h.iconLen==Qeapp::ICON_BYTES);
 if((!strcmp(m.type,"text")||!strcmp(m.type,"lua"))&&!h.payloadLen){error="Script/text app has no payload";return false;}
#if defined(VQEAF_ENABLE_LUA) && VQEAF_ENABLE_LUA
 if(!strcmp(m.type,"lua")&&h.payloadLen>64*1024){error="Lua script too large";return false;}
#endif
 if(!strcmp(m.type,"web")&&h.payloadLen){error="Web app must not contain payload";return false;}
 if(!verify)return true;
 const uint32_t sections[2]={h.iconLen,h.payloadLen};const uint8_t *hashes[2]={h.iconHash,h.payloadHash};
 uint8_t chunk[512];
 for(int part=0;part<2;part++){
   Qeapp::Sha256 checker;uint32_t left=sections[part];
   while(left){size_t take=left>sizeof chunk?sizeof chunk:left;int got=f.read(chunk,take);if(got!=(int)take){error="Truncated package contents";return false;}checker.update(chunk,take);signedBytes.update(chunk,take);left-=take;
#if defined(ARDUINO) && !defined(QEAPP_HOST_OPENSSL) && !defined(QEAPP_HOST_STUB_CRYPTO)
   yield();
#endif
   }
   checker.finish(digest);if(!Qeapp::equalHash(digest,hashes[part])){error=part==0?"Icon SHA-256 mismatch":"Payload SHA-256 mismatch";return false;}
 }
 uint8_t trailer[Qeapp::SIGNATURE_BYTES];
 if(f.read(trailer,sizeof trailer)!=(int)sizeof trailer){error="Missing QEAPP signature";return false;}
 signedBytes.finish(digest);
 if(!Qeapp::verifySignature(digest,trailer,reason)){error=reason;return false;}
 error="";return true;
}

bool AppInstallerService::inspect(const String &pkg,Qeapp::Meta &meta,String &error){
 if(!card||!card->mounted()){error="microSD not mounted";return false;}
 // Accept user-chosen microSD path, never install directories.
 String path=pkg;path.toLowerCase();if(!path.endsWith(".qeapp")||!pkg.startsWith("/")){error="Expected .qeapp file";return false;}
 File f=card->fs().open(pkg,FILE_READ);Qeapp::Header h;bool ok=scanPackage(f,h,meta,error,true);if(f)f.close();return ok;
}

bool AppInstallerService::copySection(File &src,const String &dst,uint32_t bytes,const uint8_t hash[32],String &error){
 fs::FS &fs=card->fs();File to=fs.open(dst,FILE_WRITE);if(!to){error="Cannot create installed file";return false;}
 Qeapp::Sha256 checker;uint8_t buffer[512];uint32_t left=bytes;bool valid=true;
 while(left){size_t take=left>sizeof buffer?sizeof buffer:left;int got=src.read(buffer,take);
   if(got!=(int)take||to.write(buffer,take)!=take){error="SD copy failed";valid=false;break;}
   checker.update(buffer,take);left-=take;
   progressCopied+=take;
   if(progressFn&&progressTotal){
     uint8_t percent=uint8_t(10+(uint64_t(progressCopied)*65U/progressTotal));
     if(percent>=lastProgress+4)progress(percent,"Copying signed app");
   }
#if defined(ARDUINO) && !defined(QEAPP_HOST_OPENSSL) && !defined(QEAPP_HOST_STUB_CRYPTO)
   yield();
#endif
 }
 to.flush();to.close();
 if(!valid){fs.remove(dst);return false;}
 uint8_t computed[32];checker.finish(computed);
 if(!Qeapp::equalHash(computed,hash)){fs.remove(dst);error="SHA-256 changed during install";return false;}
 return true;
}

// A signed receipt binds all installed bytes to a trusted signer. Files modified
// directly on removable SD do not become trusted merely because they reside in
// Installed/. The receipt is always rechecked before an app is launched.
bool AppInstallerService::verifyDirectory(const String &dir,const String &id,Qeapp::Meta &meta,String &error) const {
 if(!card||!card->mounted()||!safeId(id)){error="Invalid app ID or microSD";return false;}
 fs::FS &fs=card->fs();
 File receipt=fs.open(dir+"/receipt.bin",FILE_READ);
 if(!receipt||receipt.isDirectory()||receipt.size()!=Qeapp::HEADER_BYTES+Qeapp::SIGNATURE_BYTES){
   if(receipt)receipt.close();
   error="Missing signed install receipt";return false;
 }
 uint8_t record[Qeapp::HEADER_BYTES+Qeapp::SIGNATURE_BYTES];
 bool readOk=receipt.read(record,sizeof record)==(int)sizeof record;receipt.close();
 if(!readOk){error="Invalid install receipt";return false;}
 uint8_t *hdr=record,*trailer=record+Qeapp::HEADER_BYTES;
 uint64_t pkgSize=Qeapp::HEADER_BYTES+uint64_t(hdr[8])+(uint64_t(hdr[9])<<8)+
   (uint64_t(hdr[10])<<16)+(uint64_t(hdr[11])<<24)+
   uint64_t(hdr[12])+(uint64_t(hdr[13])<<8)+(uint64_t(hdr[14])<<16)+(uint64_t(hdr[15])<<24)+
   uint64_t(hdr[16])+(uint64_t(hdr[17])<<8)+(uint64_t(hdr[18])<<16)+(uint64_t(hdr[19])<<24)+Qeapp::SIGNATURE_BYTES;
 Qeapp::Header h;const char *reason="";
 if(!Qeapp::parseHeader(hdr,pkgSize,h,reason)){error=reason;return false;}
 Qeapp::Sha256 signedBytes;signedBytes.update(hdr,Qeapp::HEADER_BYTES);
 uint32_t lengths[3]={h.manifestLen,h.iconLen,h.payloadLen};
 const uint8_t *hashes[3]={h.manifestHash,h.iconHash,h.payloadHash};
 const char *names[3]={"/manifest.ini","/icon.rgb565","/payload.txt"};
 char manifest[Qeapp::MAX_MANIFEST+1]={0};uint8_t buffer[512],digest[32];
 for(int i=0;i<3;i++){
   File file=fs.open(dir+names[i],FILE_READ);
   if(lengths[i]==0){if(file){file.close();error="Unexpected unsigned section";return false;}}
   else {
     if(!file||file.isDirectory()||file.size()!=lengths[i]){if(file)file.close();error="Installed content missing/modified";return false;}
   }
   Qeapp::Sha256 section;uint32_t left=lengths[i],pos=0;
   while(left){size_t n=left>sizeof buffer?sizeof buffer:left;
     if(file.read(buffer,n)!=(int)n){file.close();error="Installed content truncated";return false;}
     section.update(buffer,n);signedBytes.update(buffer,n);
     if(i==0){memcpy(manifest+pos,buffer,n);pos+=n;}
     left-=n;
#if defined(ARDUINO) && !defined(QEAPP_HOST_OPENSSL) && !defined(QEAPP_HOST_STUB_CRYPTO)
     yield();
#endif
   }
   if(file)file.close();
   section.finish(digest);
   if(!Qeapp::equalHash(digest,hashes[i])){error="Installed content hash mismatch";return false;}
 }
 if(!Qeapp::parseManifest(manifest,h.manifestLen,meta,reason)||id!=meta.id){error="Installed manifest invalid";return false;}
 if(((!strcmp(meta.type,"text")||!strcmp(meta.type,"lua"))&&!h.payloadLen)||(!strcmp(meta.type,"web")&&h.payloadLen)){error="Installed app type mismatch";return false;}
 meta.hasIcon=h.iconLen==Qeapp::ICON_BYTES;
 signedBytes.finish(digest);
 if(!Qeapp::verifySignature(digest,trailer,reason)){error=reason;return false;}
 error="";return true;
}

bool AppInstallerService::verifyInstalled(const String &id,Qeapp::Meta &meta,String &error) const {
 return verifyDirectory(installedPath(id),id,meta,error);
}

// A power cut can happen between moving <id> to .backup-<id> and moving
// the new signed .stage-<id> into place. Prefer a verified final package;
// otherwise restore the previously verified backup. Never trust stage alone.
// No attempt is made to recover a malicious/corrupted backup or recursively
// remove user-added files: such cases require manual File Manager recovery.
AppInstallerService::RecoveryStats AppInstallerService::recoverTransactions(){
 recovery={0,0,0,0};
 if(!card||!card->mounted())return recovery;
 fs::FS &fs=card->fs();File root=fs.open(StoragePaths::APPS_INSTALLED,FILE_READ);
 if(!root||!root.isDirectory()){if(root)root.close();return recovery;}
 // Snapshot names: renaming while an SD directory iterator is active is unsafe.
 char ids[32][25]={};int n=0;
 char staged[32][25]={};int stages=0;File dir=root.openNextFile();
 while(dir){
   String name=fileName(String(dir.name()));
   if(dir.isDirectory()&&name.startsWith(".backup-")){
     String id=name.substring(8);
     if(safeId(id)){
       if(n<32)snprintf(ids[n++],sizeof ids[0],"%s",id.c_str());
       else if(recovery.blocked<255)++recovery.blocked;
     }
   }else if(dir.isDirectory()&&name.startsWith(".stage-")){
     String id=name.substring(7);
     if(safeId(id)){
       if(stages<32)snprintf(staged[stages++],sizeof staged[0],"%s",id.c_str());
       else if(recovery.blocked<255)++recovery.blocked;
     }
   }
   dir.close();dir=root.openNextFile();
 }
 root.close();
 for(int i=0;i<n;i++){
   const String id(ids[i]);
   const String finalDir=installedPath(id);
   const String backup=String(StoragePaths::APPS_INSTALLED)+"/.backup-"+id;
   Qeapp::Meta oldMeta,finalMeta;String why;
   if(!verifyDirectory(backup,id,oldMeta,why)){
     if(recovery.blocked<255)++recovery.blocked;
     continue;
   }
   bool finalValid=verifyDirectory(finalDir,id,finalMeta,why);
   if(finalValid){
     // A valid final version wins only when it is not older than backup.
     // If versions conflict, do not delete either trusted directory.
     if(Qeapp::compareVersion(finalMeta.version,oldMeta.version)<0){
       if(recovery.blocked<255)++recovery.blocked;
       continue;
     }
     if(cleanKnownFiles(backup)){
       if(recovery.finalized<255)++recovery.finalized;
     }else if(recovery.blocked<255)++recovery.blocked;
     continue;
   }
   if(fs.exists(finalDir)&&!cleanKnownFiles(finalDir)){
     if(recovery.blocked<255)++recovery.blocked;
       continue;
   }
   if(fs.rename(backup,finalDir)){
     Qeapp::Meta restored;String check;
     if(verifyInstalled(id,restored,check)){
       if(recovery.restored<255)++recovery.restored;
     } else if(recovery.blocked<255)++recovery.blocked;
   }else if(recovery.blocked<255)++recovery.blocked;
 }
 // Orphaned staging directories are never auto-installed: package consent
 // requires a fresh user confirmation. They can be discarded if they contain
 // only installer-owned files; unexpected files require manual inspection.
 for(int i=0;i<stages;i++){
   String id(staged[i]);
   String pending=String(StoragePaths::APPS_INSTALLED)+"/.stage-"+id;
   String backup=String(StoragePaths::APPS_INSTALLED)+"/.backup-"+id;
   if(fs.exists(backup))continue; // unresolved rollback, keep diagnostics
   if(cleanKnownFiles(pending)){
     if(recovery.discardedStages<255)++recovery.discardedStages;
   }else if(recovery.blocked<255)++recovery.blocked;
 }
 return recovery;
}

bool AppInstallerService::updateAvailable(const Qeapp::Meta &candidate) const {
 if(!safeId(candidate.id))return false;
 Qeapp::Meta current;String error;
 return verifyInstalled(candidate.id,current,error)&&
        Qeapp::compareVersion(candidate.version,current.version)>0;
}

void AppInstallerService::refresh(){
 used=0;if(!card||!card->mounted())return;
 recoverTransactions();
 File root=card->fs().open(StoragePaths::APPS_INSTALLED,FILE_READ);
 if(!root||!root.isDirectory())return;
 File dir=root.openNextFile();
 while(dir&&used<MAX_INSTALLED){
   String id=fileName(String(dir.name()));
   if(dir.isDirectory()&&safeId(id)){
     Qeapp::Meta verified;String reason;
     if(verifyInstalled(id,verified,reason)){
       installed[used].info=verified;
       snprintf(installed[used].path,sizeof installed[used].path,"%s",installedPath(id).c_str());used++;
     }
   }
   dir.close();dir=root.openNextFile();
 }
 if(dir)dir.close();
 root.close();
}

bool AppInstallerService::get(const String &id,Qeapp::Meta &meta) const{
 // Reverify at use-time: removable SD may have changed after listing.
 for(int i=0;i<used;i++)if(id==installed[i].info.id){String error;return verifyInstalled(id,meta,error);}
 return false;
}

bool AppInstallerService::install(const String &pkg,Qeapp::Meta &result,String &error){
 // QEAPP/2 is not a native/SIS loader. Signed, declarative web/text apps only.
 progressCopied=progressTotal=0;lastProgress=0;
 progress(0,"Verifying package signature");
 if(!inspect(pkg,result,error))return false;
 progress(5,"Checking installed version");
 const String id(result.id),finalDir=installedPath(id);
 const String stage=String(StoragePaths::APPS_INSTALLED)+"/.stage-"+id;
 const String backup=String(StoragePaths::APPS_INSTALLED)+"/.backup-"+id;
 refresh();  // also attempts to complete a previously interrupted update
 fs::FS &fs=card->fs();
 bool upgrade=false;
 if(fs.exists(finalDir)){
   Qeapp::Meta old;String reason;
   if(!verifyInstalled(id,old,reason)){
     error="Installed app damaged; recover before updating";return false;
   }
   if(Qeapp::compareVersion(result.version,old.version)<=0){
     error="Already installed. Uninstall before reinstall";
     return false;
   }
   upgrade=true;
 }else if(used>=MAX_INSTALLED){error="App catalog full (12 apps)";return false;}
 if(fs.exists(stage)||fs.exists(backup)){
   error="Pending install/update transaction; recover via App Manager";return false;
 }
 File in=fs.open(pkg,FILE_READ);Qeapp::Header h;Qeapp::Meta again;
 if(!scanPackage(in,h,again,error,false)){if(in)in.close();return false;}
 if(strcmp(again.id,result.id)||strcmp(again.version,result.version)||
    strcmp(again.name,result.name)||strcmp(again.type,result.type)||
    strcmp(again.entry,result.entry)||again.hasIcon!=result.hasIcon){
   error="Package metadata changed";in.close();return false;
 }
 // Stage and backup coexist during upgrade, so reserve room for a complete
 // new package and directory overhead. File system may over-report free space;
 // all copy errors are still checked below.
 const uint64_t required=uint64_t(h.manifestLen)+h.iconLen+h.payloadLen+4096;
 if(card->freeBytes()<required){error="Insufficient SD space for staging";in.close();return false;}
 if(!fs.mkdir(stage)){error="Cannot create staging directory";in.close();return false;}
 progressCopied=0;progressTotal=h.manifestLen+h.iconLen+h.payloadLen;
 progress(10,"Preparing staging directory");
 uint8_t headerRaw[Qeapp::HEADER_BYTES];
 if(!in.seek(0)||in.read(headerRaw,sizeof headerRaw)!=(int)sizeof headerRaw){
   error="Cannot reread package header";in.close();cleanKnownFiles(stage);return false;
 }
 bool ok=in.seek(Qeapp::HEADER_BYTES)&&
         copySection(in,stage+"/manifest.ini",h.manifestLen,h.manifestHash,error);
 if(ok&&h.iconLen)ok=copySection(in,stage+"/icon.rgb565",h.iconLen,h.iconHash,error);
 if(ok&&h.payloadLen)ok=copySection(in,stage+"/payload.txt",h.payloadLen,h.payloadHash,error);
 uint8_t trailer[Qeapp::SIGNATURE_BYTES]={0};
 if(ok&&in.read(trailer,sizeof trailer)!=(int)sizeof trailer){
   error="Missing signed trailer";ok=false;
 }
 if(ok){
   File receipt=fs.open(stage+"/receipt.bin",FILE_WRITE);
   if(!receipt||receipt.write(headerRaw,sizeof headerRaw)!=sizeof headerRaw||
      receipt.write(trailer,sizeof trailer)!=sizeof trailer){
     error="Unable to save signed receipt";ok=false;
   }
   if(receipt){receipt.flush();receipt.close();}
 }
 in.close();
 if(ok){
   progress(78,"Verifying signed staging copy");
   // Recheck the source in case SD bytes changed between inspect and copy.
   File reread=fs.open(pkg,FILE_READ);Qeapp::Header fresh;Qeapp::Meta freshMeta;
   ok=scanPackage(reread,fresh,freshMeta,error,true);
   if(ok){
     uint8_t freshHdr[Qeapp::HEADER_BYTES],freshTrailer[Qeapp::SIGNATURE_BYTES];
     uint64_t trailerOffset=uint64_t(Qeapp::HEADER_BYTES)+fresh.manifestLen+
                            fresh.iconLen+fresh.payloadLen;
     ok=reread.seek(0)&&reread.read(freshHdr,sizeof freshHdr)==(int)sizeof freshHdr&&
        reread.seek(trailerOffset)&&
        reread.read(freshTrailer,sizeof freshTrailer)==(int)sizeof freshTrailer&&
        !memcmp(headerRaw,freshHdr,sizeof headerRaw)&&
        !memcmp(trailer,freshTrailer,sizeof trailer)&&
        !strcmp(result.id,freshMeta.id)&&!strcmp(result.version,freshMeta.version);
     if(!ok)error="Signed source changed during copy";
   }
   if(reread)reread.close();
 }
 if(ok){
   Qeapp::Meta staged;String why;
   if(!verifyDirectory(stage,id,staged,why)){
     error=String("Staged signature check failed: ")+why;ok=false;
   }
 }
 if(!ok){cleanKnownFiles(stage);return false;}
 // FAT is NOT fully power-fail atomic. On reboot refresh() can restore a
 // complete, signed .backup-id when final is missing/incomplete.
 progress(91,upgrade?"Saving previous app version":"Activating new app");
 if(upgrade&&!fs.rename(finalDir,backup)){
   error="Cannot preserve old version";cleanKnownFiles(stage);return false;
 }
 if(!fs.rename(stage,finalDir)){
   error="Cannot activate staged version";
   if(upgrade&&!fs.rename(backup,finalDir))error=error+"; recover on next boot";
   cleanKnownFiles(stage);return false;
 }
 Qeapp::Meta committed;String verifyError;
 if(!verifyInstalled(id,committed,verifyError)){
   error=String("Activated version failed integrity check: ")+verifyError;
   if(cleanKnownFiles(finalDir)&&upgrade){
     if(!fs.rename(backup,finalDir))error=error+"; rollback requires reboot";
   }else if(upgrade)error=error+"; manual recovery required";
   refresh();return false;
 }
 result=committed;
 progress(97,"Refreshing applications catalog");
 // Cleanup is best effort: a backup left behind is a *verified* old app,
 // not permission to erase unexpected files. refresh() will retry next boot.
 if(upgrade)cleanKnownFiles(backup);
 refresh();error="";progress(100,"Application ready");return true;
}

bool AppInstallerService::uninstall(const String &id,String &error){
 if(!card||!card->mounted()||!safeId(id)){error="Invalid uninstall target";return false;}
 Qeapp::Meta entry;if(!get(id,entry)){error="App not installed";return false;}
 String dir=installedPath(id);
 // Do not delete unexpected files: an app directory must still contain only
 // installer-known files. Unexpected files mean manual recovery is required.
 File folder=card->fs().open(dir,FILE_READ);if(!folder||!folder.isDirectory()){error="Installed directory missing";return false;}
 File f=folder.openNextFile();bool unexpected=false;
 while(f){String n=fileName(String(f.name()));if(f.isDirectory()||(n!="manifest.ini"&&n!="icon.rgb565"&&n!="payload.txt"&&n!="receipt.bin"))unexpected=true;f.close();if(unexpected)break;f=folder.openNextFile();}if(f)f.close();folder.close();
 if(unexpected){error="Unexpected app data; uninstall stopped";return false;}
 if(!cleanKnownFiles(dir)){error="Uninstall failed; check SD card";return false;}
 refresh();error="";return true;
}

bool AppInstallerService::loadIcon(const String &id,uint16_t out[1024]){
 if(!out||!card||!card->mounted()||!safeId(id))return false;
 Qeapp::Meta entry;if(!get(id,entry)||!entry.hasIcon)return false;
 File f=card->fs().open(installedPath(id)+"/icon.rgb565",FILE_READ);
 if(!f||f.size()!=Qeapp::ICON_BYTES){if(f)f.close();return false;}
 bool ok=f.read(reinterpret_cast<uint8_t*>(out),Qeapp::ICON_BYTES)==(int)Qeapp::ICON_BYTES;f.close();return ok;
}

bool AppInstallerService::previewIcon(const String &pkg,uint16_t out[1024]){
 if(!out||!card||!card->mounted())return false;
 File f=card->fs().open(pkg,FILE_READ);Qeapp::Header h;Qeapp::Meta meta;String error;
 if(!scanPackage(f,h,meta,error,true)||!h.iconLen){if(f)f.close();return false;}
 bool ok=f.seek(Qeapp::HEADER_BYTES+h.manifestLen)&&f.read(reinterpret_cast<uint8_t*>(out),Qeapp::ICON_BYTES)==(int)Qeapp::ICON_BYTES;
 f.close();return ok;
}
