#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();

    ~HttpResponse();

    void Init(const std::string &srcDir, std::string &path, bool isKeepAlive = false, int code = -1);

    void MakeResponse(Buffer &buff);

    void UnmapFile();

    char *File();

    size_t FileLen() const;

    void ErrorContent(Buffer &buff, std::string message);

    int Code() const { return code_; }

private:
    void AddStateLine_(Buffer &buff);

    void AddHeader_(Buffer &buff);

    void AddContent_(Buffer &buff);

    void ErrorHtml_();

    std::string GetFileType_();

    int code_;              //HTTP状态码
    bool isKeepAlive_;      //是否保持连接

    std::string path_;      //请求资源路径
    std::string srcDir_;    //静态资源目录

    char *mmFile_;          //映射的文件指针
    struct stat mmFileStat_;//映射文件的stat结构体

    static const std::unordered_map <std::string, std::string> SUFFIX_TYPE;     //文件后缀与Content-Type的对应关系
    static const std::unordered_map<int, std::string> CODE_STATUS;              //HTTP状态码与状态文本的对应关系
    static const std::unordered_map<int, std::string> CODE_PATH;                //HTTP状态码与错误页面路径的对应关系
};


#endif //HTTP_RESPONSE_H