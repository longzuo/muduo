// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


// 为什么需要 mutex和条件变量
// 当此对象执行startLoop之后， 就启动了一个looper.loop
// 而且会返回一个eventloop对象，但是返回的eventloop对象的生存期是从新线程的启动
// 函数开始的，所以需要一个条件标量，让主线程等待新线程生成loop成功之后， 再返回
// 另外还可以传入一个callback函数，可以在loop执行前，在新线程中执行
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this), name),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL)
    {
      cond_.wait();
    }
  }

  return loop_;
}

void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
		//loop_是类的成员对象
		//但是loop是另外一个线程的栈变量，线程结束，生命期结束，为什么要把栈变量的地
		//址返回给主线程？可能是需要在主线程中调用loop.quit()。
		//为什么需要在新线程中生成栈变量的eventloop？
		//这样是为了让eventloop 属于新线程，在任意线程中通过调用
		//loop.isInLoopThread()函数，可以判断当前线程是不是IO线程
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();
  //assert(exiting_);
  loop_ = NULL;
}

