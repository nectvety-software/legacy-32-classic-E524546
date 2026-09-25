#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <cstring>
#include "../../src/services/ThemeFileService.h"
std::map<std::string,std::string> fakeThemeFiles;
static fs::FS disk;

bool StorageService::begin() { ok = true; return true; }
fs::FS &StorageService::fs() { return disk; }
int StorageService::scanMedia(const String &root, const char *const *extensions,
                              int extCount, FsEntry *out, int limit, int depth) {
    int n=0;
    std::string base(root.c_str());
    for (auto &item : fakeThemeFiles) {
        const std::string &path = item.first;
        if (base != "/" && path.compare(0,base.size()+1,base + "/") != 0) continue;
        const std::string tail = base == "/" ? path.substr(1) : path.substr(base.size()+1);
        if (tail.empty() || std::count(tail.begin(),tail.end(),'/') > depth) continue;
        bool match = false;
        for (int k=0;k<extCount;k++) {
            size_t extlen=strlen(extensions[k]);
            if (path.size() >= extlen && path.compare(path.size()-extlen,extlen,extensions[k])==0) match=true;
        }
        if (!match) continue;
        if (n == limit) break;
        std::string name = path.substr(path.find_last_of('/')+1);
        out[n++] = FsEntry(name, path, false, item.second.size());
    }
    return n;
}

std::string readFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { std::cerr << "Cannot read " << path << '\n'; std::exit(2); }
    std::ostringstream s; s << file.rdbuf(); return s.str();
}
static uint16_t rgb(int r,int g,int b) {
    return (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));
}
int main(int argc,char **argv) {
    if(argc < 3) return 2;
    fakeThemeFiles["/Themes/amoled_red.vqeaf"] = readFile(argv[1]);
    fakeThemeFiles["/Themes/s60_green.vqeaf"] = readFile(argv[2]);
    fakeThemeFiles["/Others/blue.vqeaf"] =
      "@vqeaf 1.0\n<theme id=\"blue\" name=\"Night Blue\">\n"
      "palette\n{\nscreen: \"#101020\"\nkeyText: \"#FFFFFF\"\naccent: \"#2050DD\"\ncustomFont: \"S60 Bitmap\"\n}\n</theme>\n";
    fakeThemeFiles["/Themes/bad.vqeaf"] =
      "@vqeaf 1.0\n<theme name=\"bad\">\npalette {\nscreen: \"#wat\"\nkeyText: \"#FFFFFF\"\naccent: \"#FF0000\"\n}\n</theme>\n";
    fakeThemeFiles["/Themes/huge.vqeaf"] = std::string(33000,'x');
    // UTF-8 BOM accepted even for first magic line.
    fakeThemeFiles["/Themes/bom.vqeaf"] =
      "\xEF\xBB\xBF@vqeaf 1.0\n<theme name=\"BOM file\">\npalette {\nscreen: \"#123456\"\nkeyText: \"#FFFFFF\"\naccent: \"#FF0000\"\n}\n</theme>\n";
    // A valid theme in a deeply nested File Manager folder is NOT scanned by
    // the bounded catalog, but direct selection must still load/apply it.
    fakeThemeFiles["/Documents/Custom/Nested/Palette/deep.vqeaf"] =
      "@vqeaf 1.0\n<theme name=\"Deep theme\">\npalette {\n"
      "screen: \"#123456\"\nkeyText: \"#FFFFFF\"\naccent: \"#206040\"\n}\n</theme>\n";
    StorageService sd; assert(sd.begin());
    ThemeFileService service; int count=service.scan(sd);
    assert(service.hasCard() && count == 4); // huge excluded, duplicates removed
    assert(service.find("/Themes/bad.vqeaf") < 0);
    int red = service.find("/Themes/amoled_red.vqeaf");
    int green = service.find("/Themes/s60_green.vqeaf");
    assert(red >= 0 && green >= 0);
    assert(service.at(red).label[0] == 'A');
    ThemeColors c = themeFor(ThemeId::Black);
    String title,error;
    assert(service.load(sd,"/Themes/amoled_red.vqeaf",c,title,error));
    assert(title == "AMOLED Red");
    assert(c.bg == rgb(0,0,0) && c.accent == rgb(255,61,91));
    assert(c.selected == rgb(56,22,27) && c.chrome == rgb(20,20,20));
    assert(service.load(sd,"/Themes/s60_green.vqeaf",c,title,error));
    assert(c.bg == rgb(140,200,34) && c.text == rgb(0,0,0));
    assert(c.chrome == rgb(40,108,24));
    // A partial palette keeps coherent S60 defaults, not black AMOLED popups.
    assert(c.popup == c.panel);
    assert(service.load(sd,"/Others/blue.vqeaf",c,title,error));
    assert(c.bg == rgb(16,16,32) && c.accent == rgb(32,80,221));
    assert(service.load(sd,"/Themes/bom.vqeaf",c,title,error));
    assert(c.bg == rgb(18,52,86));
    assert(service.find("/Documents/Custom/Nested/Palette/deep.vqeaf") < 0);
    assert(service.load(sd,"/Documents/Custom/Nested/Palette/deep.vqeaf",c,title,error));
    assert(title == "Deep theme" && c.bg == rgb(18,52,86));
    ThemeColors snapshot = c;
    assert(!service.load(sd,"/Themes/bad.vqeaf",c,title,error));
    assert(c.bg == snapshot.bg && c.text == snapshot.text);
    assert(!service.load(sd,"/Themes/huge.vqeaf",c,title,error));
    assert(!service.load(sd,"/Themes/../bad.vqeaf",c,title,error));
    assert(!service.load(sd,"/Themes/missing.vqeaf",c,title,error));
    assert(!service.load(sd,"/Themes/amoled_red.txt",c,title,error));
    std::cout << "PASS: scan 4 validated themes (malformed skipped), dedup, palette imports, BOM,"
              << " safe fallback, invalid/oversized/path rejection\n";
}
