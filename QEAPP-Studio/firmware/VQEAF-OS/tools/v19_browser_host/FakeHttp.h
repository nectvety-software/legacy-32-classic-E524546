#pragma once
#include <string>
#include <cstddef>
struct FakeHttpState {
 int status=200;
 int size=-1;
 std::string body;
 size_t offset=0;
 bool stuck=false;
 std::string lastUrl;
 std::string contentType="text/html";
 std::string transferEncoding;
 void reset(int st,int declared,const std::string &data,bool keep=false){
  status=st;size=declared;body=data;offset=0;stuck=keep;
 }
};
extern FakeHttpState gFakeHttp;
