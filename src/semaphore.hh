#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>
#include <node.h>

class Semaphore : public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> exports, v8::Handle<v8::Object> module);
 private:
  Semaphore(char *name);
  ~Semaphore();

  sem_t *sem_;
  char *name_;

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Unlink(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> Wait(const v8::Arguments& args);
  static v8::Handle<v8::Value> TryWait(const v8::Arguments& args);
  static v8::Handle<v8::Value> Post(const v8::Arguments& args);
  static v8::Handle<v8::Value> Name(const v8::Arguments& args);
  static v8::Persistent<v8::Function> constructor;
};

#endif
