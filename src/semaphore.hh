#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>
#include <uv.h>
#include <node.h>

class Semaphore : public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> exports, v8::Handle<v8::Object> module);
 private:
  Semaphore(char *name);
  ~Semaphore();

  sem_t *sem_;
  char *name_;

  bool waitAsyncRunning_;
  uv_async_t waitAsyncWatcher_;
  pthread_t waitAsyncThread_;
  v8::Persistent<v8::Function> waitAsyncCallback_;

  int Wait(void);
  void WaitAsyncCleanup(void);
  void WaitAsyncCancel(void);
  void WaitAsyncCallbackRun (v8::Handle<v8::Value> result);

  static void WaitAsyncCallback (uv_async_t *watcher, int revents);
  static void* WaitAsyncThread(void *arg);
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Unlink(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> WaitAsyncStart(const v8::Arguments& args);
  static v8::Handle<v8::Value> WaitAsyncCancel(const v8::Arguments& args);
  static v8::Handle<v8::Value> WaitSync(const v8::Arguments& args);
  static v8::Handle<v8::Value> TryWait(const v8::Arguments& args);
  static v8::Handle<v8::Value> Post(const v8::Arguments& args);
  static v8::Handle<v8::Value> Name(const v8::Arguments& args);
  static v8::Persistent<v8::Function> constructor;
};

#endif
