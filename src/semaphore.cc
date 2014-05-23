#ifndef BUILDING_NODE_EXTENSION
  #define BUILDING_NODE_EXTENSION
#endif

#include <errno.h>
#include <string.h>
#include <uv.h>
#include <pthread.h>
#include <node.h>
#include "semaphore.hh"

using namespace v8;

Persistent<Function> Semaphore::constructor;

Semaphore::Semaphore(char *name) {
  name_ = (char *)calloc(sizeof(char), strlen(name));
  strcpy(name_, name);

  uv_async_init(uv_default_loop(), &waitAsyncWatcher_, Semaphore::WaitAsyncCallback);
  waitAsyncWatcher_.data = this;
  waitAsyncRunning_ = false;
}

Semaphore::~Semaphore() {
  free(name_);
  if (waitAsyncRunning_)
    WaitAsyncCleanup();
}

void Semaphore::Init(Handle<Object> exports, Handle<Object> module) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("Semaphore"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  NODE_SET_PROTOTYPE_METHOD(tpl, "unlink", Unlink);
  NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(tpl, "wait", WaitAsyncStart);
  NODE_SET_PROTOTYPE_METHOD(tpl, "waitCancel", WaitAsyncCancel);
  NODE_SET_PROTOTYPE_METHOD(tpl, "waitSync", WaitSync);
  NODE_SET_PROTOTYPE_METHOD(tpl, "tryWait", TryWait);
  NODE_SET_PROTOTYPE_METHOD(tpl, "post", Post);
  NODE_SET_PROTOTYPE_METHOD(tpl, "name", Name);
  constructor = Persistent<Function>::New(tpl->GetFunction());

  module->Set(String::NewSymbol("exports"), constructor);
}

Handle<Value> Semaphore::New(const Arguments& args) {
  HandleScope scope;

  if (args.IsConstructCall()) {
    // Invoked as constructor: `new Semaphore(...)`
    if (args.Length() < 1)
      return ThrowException(Exception::SyntaxError(String::New("Semaphore::New -> Requires a name")));
    if (!args[0]->IsString())
      return ThrowException(Exception::TypeError(String::New("Semaphore::New -> Name must be a string")));
    String::Utf8Value name(args[0]->ToString());
    int initialValue = 1;
    if (args.Length() >= 2) {
      if (!args[1]->IsNumber())
        return ThrowException(Exception::TypeError(String::New("Semaphore::New -> Initial value must be an integer")));
      initialValue = args[1]->IntegerValue();
    }
    Semaphore* obj = new Semaphore(*name);
    obj->sem_ = sem_open(obj->name_, O_CREAT, 0666, initialValue);
    if (obj->sem_ == SEM_FAILED)
      return ThrowException(Exception::Error(String::New("Semaphore::New -> Could not create semaphore")));
    obj->Wrap(args.This());
    return args.This();
  } else {
    // Invoked as plain function `Semaphore(...)`, turn into construct call.
    const int argc = 1;
    Local<Value> argv[argc] = { args[0] };
    return scope.Close(constructor->NewInstance(argc, argv));
  }
}

Handle<Value> Semaphore::Unlink(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_unlink(obj->name_) < 0) {
    return ThrowException(node::ErrnoException(errno, "Semaphore::Unlink", ""));
  }
  return Undefined();
}

Handle<Value> Semaphore::Close(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_close(obj->sem_) < 0) {
    return ThrowException(Exception::Error(String::New("Semaphore::Close -> Failed")));
  }
  return Undefined();
}

int Semaphore::Wait() {
  return sem_wait(sem_);
}

void Semaphore::WaitAsyncCleanup() {
  waitAsyncCallback_.Dispose();
  waitAsyncCallback_.Clear();
  waitAsyncRunning_ = false;
}

void Semaphore::WaitAsyncCallbackRun(Handle<Value> result) {
  HandleScope scope;
  // Stash locally so that the persistent handle can be disposed safely
  Local<Function> callback = Local<Function>::New(waitAsyncCallback_);
  WaitAsyncCleanup();
  Handle<Value> args[1] = { result };
  callback->Call(handle_, 1, args);
}

void Semaphore::WaitAsyncCallback (uv_async_t *watcher, int revents) {
  HandleScope scope;
  Semaphore* obj = (Semaphore *)watcher->data;
  obj->WaitAsyncCallbackRun(True());
}

void* Semaphore::WaitAsyncThread(void *arg) {
  Semaphore* obj = (Semaphore *)arg;
  obj->Wait();
  uv_async_send(&obj->waitAsyncWatcher_);
  return obj;
}

Handle<Value> Semaphore::WaitAsyncStart(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (args.Length() < 1 || !args[0]->IsFunction())
    return ThrowException(Exception::SyntaxError(String::New("Semaphore::Wait -> Requires a function callback")));
  if (obj->waitAsyncRunning_)
    return ThrowException(Exception::Error(String::New("Semaphore::Wait -> Wait already in progress")));
  obj->waitAsyncRunning_ = true;
  obj->waitAsyncCallback_ = Persistent<Function>::New(Local<Function>::Cast(args[0]));
  // Use a pthread since libuv doesn't guarantee that uv_thread is pthread, and we need pthread_cancel/kill
  pthread_create (&obj->waitAsyncThread_, NULL, Semaphore::WaitAsyncThread, obj);
  return args.This();
}

void Semaphore::WaitAsyncCancel() {
  if (waitAsyncRunning_) {
    pthread_cancel(waitAsyncThread_);
    pthread_kill(waitAsyncThread_, SIGINT);
    pthread_join(waitAsyncThread_, NULL);
    WaitAsyncCallbackRun(False());
  }
}

Handle<Value> Semaphore::WaitAsyncCancel(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  obj->WaitAsyncCancel();
  return args.This();
}

Handle<Value> Semaphore::WaitSync(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (obj->Wait() < 0) {
    return ThrowException(node::ErrnoException(errno, "Semaphore::WaitSync", ""));
  }
  return True();
}

Handle<Value> Semaphore::TryWait(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_trywait(obj->sem_) < 0) {
    if (errno == EAGAIN)
      return False();
    else
      return ThrowException(node::ErrnoException(errno, "Semaphore::TryWait", ""));
  }
  return True();
}

Handle<Value> Semaphore::Post(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_post(obj->sem_) < 0) {
    return ThrowException(node::ErrnoException(errno, "Semaphore::Post", ""));
  }
  return True();
}

Handle<Value> Semaphore::Name(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  return scope.Close(String::New(obj->name_));
}

NODE_MODULE(semaphore, Semaphore::Init)
