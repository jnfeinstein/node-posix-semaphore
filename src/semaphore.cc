#ifndef BUILDING_NODE_EXTENSION
  #define BUILDING_NODE_EXTENSION
#endif

#include <errno.h>
#include <string.h>
#include <node.h>
#include "semaphore.hh"

using namespace v8;

Persistent<Function> Semaphore::constructor;

Semaphore::Semaphore(char *name) {
  name_ = (char *)calloc(sizeof(char), strlen(name));
  strcpy(name_, name);
}

Semaphore::~Semaphore() {
  free(name_);
}

void Semaphore::Init(Handle<Object> exports, Handle<Object> module) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("Semaphore"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  NODE_SET_PROTOTYPE_METHOD(tpl, "unlink", Unlink);
  NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(tpl, "wait", Wait);
  NODE_SET_PROTOTYPE_METHOD(tpl, "trywait", TryWait);
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
    switch errno {
      case EACCES:
        return ThrowException(Exception::Error(String::New("Semaphore::Unlink -> Insufficient permissions")));
      case ENOENT:
        return ThrowException(Exception::Error(String::New("Semaphore::Unlink -> Semaphore does not exist")));
      default:
        return ThrowException(Exception::Error(String::New("Semaphore::Unlink -> Failed")));
    }
  }
  return scope.Close(Undefined());
}

Handle<Value> Semaphore::Close(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_close(obj->sem_) < 0) {
    return ThrowException(Exception::Error(String::New("Semaphore::Close -> Failed")));
  }

  return scope.Close(Undefined());
}

Handle<Value> Semaphore::Wait(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_wait(obj->sem_) < 0) {
    switch(errno) {
      case EDEADLK:
        return ThrowException(Exception::Error(String::New("Semaphore::Wait -> Deadlock detected")));
      case EINTR:
        return ThrowException(Exception::Error(String::New("Semaphore::Wait -> Interrupted by system signal")));
      case EINVAL:
        return ThrowException(Exception::Error(String::New("Semaphore::Wait -> Semaphore does not exist")));
      default:
        return ThrowException(Exception::Error(String::New("Semaphore::Wait -> Failed")));
    }
  }
  return scope.Close(Boolean::New(true));
}

Handle<Value> Semaphore::TryWait(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_trywait(obj->sem_) < 0) {
    switch(errno) {
      case EAGAIN:
        return scope.Close(Boolean::New(false));
      case EDEADLK:
        return ThrowException(Exception::Error(String::New("Semaphore::TryWait -> Deadlock detected")));
      case EINTR:
        return ThrowException(Exception::Error(String::New("Semaphore::TryWait -> Interrupted by system signal")));
      case EINVAL:
        return ThrowException(Exception::Error(String::New("Semaphore::TryWait -> Semaphore does not exist")));
      default:
        return ThrowException(Exception::Error(String::New("Semaphore::TryWait -> Failed")));
    }
  }
  return scope.Close(Boolean::New(true));
}

Handle<Value> Semaphore::Post(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  if (sem_post(obj->sem_) < 0) {
    switch(errno) {
      case EINVAL:
        return ThrowException(Exception::Error(String::New("Semaphore::TryWait -> Semaphore does not exist")));
      default:
        return ThrowException(Exception::Error(String::New("Semaphore::TryWait -> Failed")));
    }
  }
  return scope.Close(Boolean::New(true));
}

Handle<Value> Semaphore::Name(const Arguments& args) {
  HandleScope scope;

  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(args.This());
  return scope.Close(String::New(obj->name_));
}

NODE_MODULE(semaphore, Semaphore::Init)
