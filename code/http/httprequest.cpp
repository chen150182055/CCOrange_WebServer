#include "httprequest.h"

using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
        "/index", "/register", "/login",
        "/welcome", "/video", "/picture",};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
        {"/register.html", 0},
        {"/login.html",    1},};

/**
 * @brief 初始化 HttpRequest 对象的成员变量
 */
void HttpRequest::Init() {
    //将 method_、path_、version_ 和 body_ 清空
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    //清空 header_ 和 post_ 两个无序 map
    header_.clear();
    post_.clear();
}

/**
 * @brief 检查当前请求是否是一个“保持连接（keep-alive）”的请求
 * @return
 */
bool HttpRequest::IsKeepAlive() const {
    if (header_.count("Connection") == 1) {
        //如果Connection字段的值为keep-alive并且请求版本是1.1，则认为请求是一个保持连接的请求
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    //如果请求头中没有Connection字段，则认为请求不是保持连接的
    return false;
}

/**
 * @brief  HTTP 请求的解析函数（状态机）
 * @param buff Buffer 存放着接收到的 HTTP 请求数据
 * @return
 */
bool HttpRequest::parse(Buffer &buff) {
    const char CRLF[] = "\r\n";
    //判断是否有数据
    if (buff.ReadableBytes() <= 0) {
        return false;   //没用则返回false
    }
    //进入 while 循环进行数据解析
    while (buff.ReadableBytes() && state_ != FINISH) {
        //每次取出从当前读指针开始到CRLF标志的一行数据（这里用search函数查找），并将其保存在line字符串中
        const char *lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);
        //对不同状态下的数据进行解析
        //状态主要有四种，分别是 REQUEST_LINE、HEADERS、BODY 和 FINISH
        switch (state_) {
            case REQUEST_LINE:
                //则解析请求行（请求方法、请求路径、HTTP版本号），并设置状态为HEADERS
                if (!ParseRequestLine_(line)) {
                    return false;
                }
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if (buff.ReadableBytes() <= 2) {
                    state_ = FINISH;
                }
                break;
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if (lineEnd == buff.BeginWrite()) { break; }
        //每次解析完一行数据后，通过RetrieveUntil()函数从缓冲区中移除已读数据
        buff.RetrieveUntil(lineEnd + 2);
    }
    //输出解析结果（请求方法、请求路径、HTTP版本号）
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

/**
 * @brief 根据请求的 path_ 解析出请求的文件路径
 */
void HttpRequest::ParsePath_() {
    //如果请求的 path_ 为根路径 /，则将文件路径设置为 /index.html
    if (path_ == "/") {
        path_ = "/index.html";
    } else {
        //如果请求的 path_ 在默认 HTML 文件集合 DEFAULT_HTML 中，那么将文件路径修改为以 .html 结尾的格式
        //DEFAULT_HTML 是一个无序集合，用于存储默认的 HTML 文件路径
        //默认情况下，如果请求的 path_ 不在 DEFAULT_HTML 集合中，则默认请求的是静态文件，例如图片、CSS 样式表等
        for (auto &item: DEFAULT_HTML) {    //使用 for 循环遍历默认 HTML 文件集合 DEFAULT_HTML
            //如果在集合中找到请求的 path_，则将文件路径修改为以 .html 结尾的格式
            if (item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

/**
 * @brief 解析 HTTP 请求的第一行（请求行），并设置状态为 HEADERS
 * 使用了C++11正则表达式库 regex匹配请求行中的三个字段：
 * 请求方法（如 GET、POST、PUT 等）、
 * 请求路径（即 URI）、
 * HTTP 版本号（如 1.0、1.1、2.0 等
 * @param line
 * @return
 */
bool HttpRequest::ParseRequestLine_(const string &line) {
    //使用正则表达式 ^([^ ]*) ([^ ]*) HTTP/([^ ]*)$ 进行匹配
    /*
     * ^ 表示匹配字符串开头
     * ([^ ]*) 表示匹配除空格外的任意字符，且尽可能少匹配（即非贪婪模式），并将匹配的结果存储在 subMatch 中
     * 表示匹配一个空格
     * HTTP/ 表示匹配字符串中的 HTTP/ 字符串
     * ([^ ]*) 同上
     * $ 表示匹配字符串结尾
     */
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        //将 subMatch 中的三个结果分别存储到 method_、path_ 和 version_ 中
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        //将 state_ 设置为 HEADERS，表示接下来要解析请求头
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

/**
 * @brief 解析 HTTP 请求中的头部
 * @param line 表示HTTP请求头部的行的字符串
 */
void HttpRequest::ParseHeader_(const string &line) {
    /*
     * 例如，对于一个 HTTP 请求头部中的行 "Host: www.example.com"，
     * 该函数将解析出 "Host" 和 "www.example.com" 两个部分，
     * 并将它们存储在 header_ 的一个键值对中，即 header_["Host"] = "www.example.com"
     */
    //使用正则表达式 ^([^:]*): ?(.*)$ 进行匹配
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    //根据行内容将头部信息存储在 header_ 数据成员中
    if (regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    } else {
        state_ = BODY;
    }
}

/**
 * @brief 解析HTTP请求的body部分
 * @param line
 */
void HttpRequest::ParseBody_(const string &line) {
    //将body_成员设置为line参数，即将HTTP请求中body部分的数据存储在HttpRequest实例的body_成员中
    body_ = line;
    //ParsePost_()函数解析post请求中提交的数据
    ParsePost_();
    //将state_成员设置为FINISH，表示HTTP请求的解析已经完成
    state_ = FINISH;
    //记录日志输出HTTP请求的body部分和长度
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

/**
 * @brief 将十六进制字符转换为整数的功能
 * 这个函数通常在解析 POST 请求中的表单数据时使用，
 * 因为表单数据的 key-value 对中的 value 可能会以 URL 编码的方式提交，需要将其解码
 * @param ch 输入参数是一个字符，表示一个十六进制数
 * @return
 */
int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

/**
 * @brief 解析POST请求中的数据，并根据请求的URL path 和 POST 数据中的内容，
 * 判断是否有特定的关键字（即 "DEFAULT_HTML_TAG" 字典中是否存在相应的键值），
 * 然后执行特定的操作
 */
void HttpRequest::ParsePost_() {
    //如果当前是 POST 请求且 Content-Type 为 application/x-www-form-urlencoded
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        //调用 ParseFromUrlencoded_() 方法解析 POST 数据
        ParseFromUrlencoded_();
        //如果在 DEFAULT_HTML_TAG 字典中找到了请求的 URL path 对应的键
        if (DEFAULT_HTML_TAG.count(path_)) {
            //说明该请求是一个特定的请求（如登录请求或者注册请求）
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                //如果是登录请求，则需要调用 UserVerify() 方法进行验证
                if (UserVerify(post_["username"], post_["password"], isLogin)) {
                    //验证通过则返回 /welcome.html 页面
                    path_ = "/welcome.html";
                } else
                    //否则返回 /error.html 页面
                    path_ = "/error.html";
            }
        }
    }
}

/**
 * @brief 解析 POST 请求中的表单数据
 * 在 HTTP POST 请求中，表单数据会被放在请求体（request body）中，
 * 并且以 URL 编码的形式传输
 */
void HttpRequest::ParseFromUrlencoded_() {
    //判断请求体是否为空
    if (body_.size() == 0) { return; }
    //key 和 value 分别用于存储键和值，num 用于存储 % 后面的十六进制数
    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; i++) {
        char ch = body_[i];
        //通过 switch-case 语句判断当前字符的类型
        switch (ch) {
            case '=':
                //将当前下标之前的部分作为键 key
                key = body_.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                //将其转换成空格
                body_[i] = ' ';
                break;
            case '%':
                //将 % 后面的两个字符转换成一个 ASCII 码
                num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
                //用该 ASCII 码代替 % 和后面的两个字符
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                //将当前下标之前的部分作为值 value
                value = body_.substr(j, i - j);
                j = i + 1;
                //将键值对存入 post_ 中
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    //如果循环结束后，当前处理的字符下标 i 没有到达请求体的末尾，说明还有一个键值对没有存入 post_，则将最后一个键值对存入 map 中
    assert(j <= i);
    if (post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

/**
 * @brief 实现了用户登录验证的逻辑
 * @param name
 * @param pwd
 * @param isLogin
 * @return
 */
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    //检查用户名和密码是否为空
    if (name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    //与MySQL数据库建立连接，并构建SQL查询语句
    MYSQL *sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;              //定义验证标志,默认为验证失败
    unsigned int j = 0;
    char order[256] = {0};          //定义 SQL 语句的字符数组
    MYSQL_FIELD *fields = nullptr;  //定义 MySQL 查询结果字段对象
    MYSQL_RES *res = nullptr;       //定义 MySQL 查询结果对象

    if (!isLogin) { flag = true; }  // 如果不是登录行为，则直接返回验证成功
    /* 查询用户及密码 */
    //构造查询语句
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 执行查询语句
    if (mysql_query(sql, order)) {
        //检查用户名和密码是否与数据库中的匹配
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);  // 存储查询结果
    j = mysql_num_fields(res);            // 获取查询结果的字段数
    fields = mysql_fetch_fields(res);     // 获取查询结果的字段对象

    // 遍历查询结果
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);       // 获取密码
        if (isLogin) {                    // 如果是登录行为
            if (pwd == password) { flag = true; }   // 如果密码正确，则验证成功
            else {                        // 如果密码不正确
                flag = false;
                LOG_DEBUG("pwd error!");  // 打印密码错误信息
            }
        } else {                          // 如果是注册行为
            flag = false;
            LOG_DEBUG("user used!");      // 打印用户名已被使用信息
        }
    }
    mysql_free_result(res);         // 释放查询结果

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true) {       // 如果是注册行为且用户名未被使用
        LOG_DEBUG("regirster!");          // 打印注册成功信息
        bzero(order, 256);          // 将 SQL 语句清空
        // 构造插入语句
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);           // 打印插入语句
        //通过mysql_query()函数将新用户的信息插入到user表中
        if (mysql_query(sql, order)) {  // 执行插入语句
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    //释放MySQL连接
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    //返回flag表示用户是否验证成功
    return flag;
}

/**
 * @brief
 * @return
 */
std::string HttpRequest::path() const {
    return path_;
}

/**
 * @brief
 * @return
 */
std::string &HttpRequest::path() {
    return path_;
}

/**
 * @brief
 * @return
 */
std::string HttpRequest::method() const {
    return method_;
}

/**
 * @brief
 * @return
 */
std::string HttpRequest::version() const {
    return version_;
}

/**
 * @brief 获取HTTP请求中以POST方式提交的表单中的指定参数的值
 * @param key 需要获取的参数的名称
 * @return
 */
std::string HttpRequest::GetPost(const std::string &key) const {
    assert(key != "");
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

/**
 * @brief 从POST请求中获取特定键对应的值
 * @param key 需要获取的POST键的名称
 * @return
 */
std::string HttpRequest::GetPost(const char *key) const {
    assert(key != nullptr);         //检查key参数是否为空指针
    //检查POST键值对是否已经存储在post_映射中
    if (post_.count(key) == 1) {
        //返回对应的POST键值
        return post_.find(key)->second;
    }
    return "";
}