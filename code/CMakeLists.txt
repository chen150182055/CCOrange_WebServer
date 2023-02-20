set(SRCS
        main.cpp
        buffer/buffer.cpp
        buffer/buffer.h
        config/config.h
        http/httpconn.cpp
        http/httpconn.h
        http/httprequest.cpp
        http/httprequest.h
        http/httpresponse.cpp
        http/httpresponse.h
        log/blockqueue.h
        log/log.cpp
        log/log.h
        pool/sqlconnRAII.h
        pool/sqlconnpool.cpp
        pool/sqlconnpool.h
        pool/threadpool.h
        server/epoller.cpp
        server/epoller.h
        server/webserver.cpp
        server/webserver.h
        timer/heaptimer.cpp
        timer/heaptimer.h
        )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(Threads REQUIRED)
find_package(MYSQL REQUIRED)
add_executable(server ${SRCS})
target_link_libraries(server
        pthread
        mysqlclient
        )