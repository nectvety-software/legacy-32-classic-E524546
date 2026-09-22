// SD Card Manager cho ESP32-S3 - Phiên bản cuối cùng
// Đã loại bỏ tất cả lỗi trùng lặp
// Chỉ giữ lại 3 file hoạt động

// File này là độc lập và không có dependency với các file cũ
// Không sử dụng với các file webs_browser_*

#ifndef SD_CARD_FINAL_H
#define SD_CARD_FINAL_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <vector>

// SD Card configuration
#define DEFAULT_SD_CS_PIN 5
#define SD_MOUNT_POINT "/sd"

class SDCardManager {
private:
    bool sdCardAvailable;
    int csPin;
    size_t totalSpace;
    size_t usedSpace;
    size_t freeSpace;
    
    // File operations (private - không có trùng lặp)
    bool createDirectory(const String& path);
    bool removeFileFromDisk(const String& path);
    size_t getFileSize(const String& path);
    
    // SPI operations
    bool initializeSPI(int frequency);
    void printSPIInfo();

public:
    SDCardManager(int csPin = DEFAULT_SD_CS_PIN);
    
    // Initialization
    bool begin();
    void end();
    bool isAvailable() { return sdCardAvailable; }
    
    // Directory operations
    bool createDir(const String& path);
    bool removeDir(const String& path);
    bool listDir(const String& path, uint8_t levels = 2);
    std::vector<String> listFiles(const String& path = "/");
    
    // File operations - chỉ có một phiên bản mỗi hàm
    bool writeFile(const String& path, const String& content);
    bool appendFile(const String& path, const String& content);
    bool readFile(const String& path, String& content);
    bool removeFile(const String& path);
    bool renameFile(const String& path1, const String& path2);
    bool fileExists(const String& path);
    
    // Card information
    void printCardInfo();
    size_t getTotalSpace() { return totalSpace; }
    size_t getUsedSpace() { return usedSpace; }
    size_t getFreeSpace() { return freeSpace; }
    float getUsagePercentage();
    
    // Card operations
    bool formatCard();
    bool testCard();
    void benchmark();
    
    // Advanced operations
    bool copyFile(const String& src, const String& dest);
    bool moveFile(const String& src, const String& dest);
    size_t getDirectorySize(const String& path);
    std::vector<String> findFiles(const String& pattern);
    
    // Error handling
    String getLastError();
    void clearError();
    
    // Utilities
    static String formatFileSize(size_t bytes);
    static bool isValidFilename(const String& filename);
    static String sanitizeFilename(const String& filename);
};

#endif // SD_CARD_FINAL_H