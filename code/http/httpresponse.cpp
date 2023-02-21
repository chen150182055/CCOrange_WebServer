#include "httpresponse.h"

using namespace std;

const unordered_map <string, string> HttpResponse::SUFFIX_TYPE = {
        {".html",  "text/html"},
        {".xml",   "text/xml"},
        {".xhtml", "application/xhtml+xml"},
        {".txt",   "text/plain"},
        {".rtf",   "application/rtf"},
        {".pdf",   "application/pdf"},
        {".word",  "application/nsword"},
        {".png",   "image/png"},
        {".gif",   "image/gif"},
        {".jpg",   "image/jpeg"},
        {".jpeg",  "image/jpeg"},
        {".au",    "audio/basic"},
        {".mpeg",  "video/mpeg"},
        {".mpg",   "video/mpeg"},
        {".avi",   "video/x-msvideo"},
        {".gz",    "application/x-gzip"},
        {".tar",   "application/x-tar"},
        {".css",   "text/css "},
        {".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
        {200, "OK"},
        {400, "Bad Request"},
        {403, "Forbidden"},
        {404, "Not Found"},
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
        {400, "/400.html"},
        {403, "/403.html"},
        {404, "/404.html"},
};


/**
 * @brief 构造函数
 */
HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
};

/**
 * @brief 析构函数
 */
HttpResponse::~HttpResponse() {
    UnmapFile();
}

/**
 * @brief 初始化 HttpResponse 实例的一些数据成员
 * @param srcDir Web 服务器所提供服务的文件根目录
 * @param path 请求文件的路径
 * @param isKeepAlive 是否为长连接
 * @param code 响应码
 */
void HttpResponse::Init(const string &srcDir, string &path, bool isKeepAlive, int code) {
    assert(srcDir != "");           //判断 srcDir 是否为空
    if (mmFile_) { UnmapFile(); }   //判断之前是否有文件映射到内存中
    //将输入参数分别赋值给对应的数据成员
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

/**
 * @brief 根据响应状态码和文件路径，向输出缓冲区 buff 中添加 HTTP 响应的状态行、响应头和响应内容
 * @param buff 输出缓冲区
 */
void HttpResponse::MakeResponse(Buffer &buff) {
    /* 判断请求的资源文件 */
    //通过 stat 函数获取请求的资源文件的属性信息
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        //请求的资源文件不存在或者是一个目录，则设置响应状态码为 404（Not Found）
        code_ = 404;
    } else if (!(mmFileStat_.st_mode & S_IROTH)) {
        //请求的资源文件的访问权限不足，则设置响应状态码为 403（Forbidden）
        code_ = 403;
    } else if (code_ == -1) {
        //响应状态码没有被设置，则将响应状态码设置为 200（OK）
        code_ = 200;
    }
    ErrorHtml_();           //根据响应状态码生成对应的错误页面
    //向输出缓冲区添加状态行、响应头和响应内容
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

/**
 * @brief 返回一个指向内存映射文件的指针，用于获取响应正文的数据
 * 内存映射文件是将文件的内容映射到进程的地址空间的一种技术，
 * 通过将文件映射到内存中，可以避免频繁的磁盘 I/O 操作，提高文件读取的效率
 * @return
 */
char *HttpResponse::File() {
    return mmFile_;
}

/**
 * @brief 返回当前 HttpResponse 对象的文件大小（即被访问的资源文件大小）
 * @return
 */
size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

/**
 * @brief 在发生错误时，将对应状态码的HTML错误页面的路径设置为HttpResponse的路径，
 * 并获取该路径的文件信息
 */
void HttpResponse::ErrorHtml_() {
    if (CODE_PATH.count(code_) == 1) {
        //在CODE_PATH中查找对应状态码的路径
        path_ = CODE_PATH.find(code_)->second;
        //将该路径设置为HttpResponse的路径，并获取该路径的文件信息
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

/**
 * @brief 在HTTP响应头部分添加状态行
 * @param buff
 */
void HttpResponse::AddStateLine_(Buffer &buff) {
    string status;
    //查询CODE_STATUS这个哈希表得到code_对应的状态描述status
    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        //如果没有找到则将code_设置为400（客户端错误）
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    //将状态行添加到响应头部
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

/**
 * @brief 向 HTTP 响应报文的头部添加状态行和头部字段
 * 头部字段包括 Connection、Content-type 两个字段，
 * 其中 Connection 字段用于告知客户端是否需要保持连接，
 * Content-type 字段用于告知客户端响应内容的数据类型
 * @param buff
 */
void HttpResponse::AddHeader_(Buffer &buff) {
    buff.Append("Connection: ");
    if (isKeepAlive_) {
        //添加一个 keep-alive 字段，该字段告知客户端最多可维持 6 个 HTTP 请求/响应交互，且每个交互的超时时间为 120 秒
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.Append("close\r\n");
    }
    //Content-type 字段的取值由 GetFileType_ 函数确定
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

/**
 * @brief 将请求的资源文件映射到内存中，并将文件的内容添加到HTTP响应报文中
 * @param buff
 */
void HttpResponse::AddContent_(Buffer &buff) {
    //首先通过调用open()函数打开文件，并返回文件描述符srcFd
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0) {
        ErrorContent(buff, "File NotFound!");
        return;
    }

    /* 将文件映射到内存提高文件的访问速度
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    //通过调用mmap()函数将文件映射到内存，得到指向该内存的指针mmFile_
    int *mmRet = (int *) mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1) {
        //若文件打开或映射失败，则该函数将调用ErrorContent()函数向HTTP响应报文中添加错误信息
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char *) mmRet;
    //关闭文件
    close(srcFd);
    //将文件长度添加到HTTP响应头部，内容添加到HTTP响应体中
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

/**
 * @brief 取消当前文件的内存映射
 */
void HttpResponse::UnmapFile() {
    if (mmFile_) {
        //调用munmap()函数，将文件内存映射取消
        munmap(mmFile_, mmFileStat_.st_size);
        //将类成员mmFile_置为nullptr
        mmFile_ = nullptr;
    }
}

/**
 * @brief 获取请求资源文件的文件类型，它会根据文件的后缀名来确定文件类型
 * @return
 */
string HttpResponse::GetFileType_() {
    /* 判断文件类型 */
    //从请求资源路径中找到最后一个 "." 符号的位置
    string::size_type idx = path_.find_last_of('.');
    if (idx == string::npos) {
        //如果找不到，返回默认的类型 "text/plain"
        return "text/plain";
    }
    //如果找到了，将后缀名截取下来，再查找 SUFFIX_TYPE 表中对应的类型
    string suffix = path_.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1) {
        //如果能找到对应的类型，返回对应的类型
        return SUFFIX_TYPE.find(suffix)->second;
    }
    //否则也返回默认的类型 "text/plain"
    return "text/plain";
}

/**
 * @brief 添加错误信息的内容，主要用于在文件打开或读取失败时使用
 * @param buff
 * @param message
 */
void HttpResponse::ErrorContent(Buffer &buff, string message) {
    //生成一个包含错误信息的 HTML 页面
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";
    //然后添加到 Buffer 中作为响应的主体，并设置正确的 Content-length 头
    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
