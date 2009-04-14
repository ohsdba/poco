//
// NotificationQueue.cpp
//
// $Id: //poco/Main/Foundation/src/NotificationQueue.cpp#15 $
//
// Library: Foundation
// Package: Notifications
// Module:  NotificationQueue
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/NotificationQueue.h"
#include "Poco/NotificationCenter.h"
#include "Poco/Notification.h"
#include "Poco/SingletonHolder.h"


namespace Poco {


NotificationQueue::NotificationQueue()
{
}


NotificationQueue::~NotificationQueue()
{
	clear();
}


void NotificationQueue::enqueueNotification(Notification::Ptr pNotification)
{
	poco_check_ptr (pNotification);
	FastMutex::ScopedLock lock(_mutex);
	if (_waitQueue.empty())
	{
		_nfQueue.push_back(pNotification);
	}
	else
	{
		WaitInfo* pWI = _waitQueue.front();
		_waitQueue.pop_front();
		pWI->pNf = pNotification;
		pWI->nfAvailable.set();
	}	
}


void NotificationQueue::enqueueUrgentNotification(Notification::Ptr pNotification)
{
	poco_check_ptr (pNotification);
	FastMutex::ScopedLock lock(_mutex);
	if (_waitQueue.empty())
	{
		_nfQueue.push_front(pNotification);
	}
	else
	{
		WaitInfo* pWI = _waitQueue.front();
		_waitQueue.pop_front();
		pWI->pNf = pNotification;
		pWI->nfAvailable.set();
	}	
}


Notification* NotificationQueue::dequeueNotification()
{
	FastMutex::ScopedLock lock(_mutex);
	return dequeueOne().duplicate();
}


Notification* NotificationQueue::waitDequeueNotification()
{
	Notification::Ptr pNf;
	WaitInfo* pWI = 0;
	{
		FastMutex::ScopedLock lock(_mutex);
		pNf = dequeueOne();
		if (pNf) return pNf.duplicate();
		pWI = new WaitInfo;
		_waitQueue.push_back(pWI);
	}
	pWI->nfAvailable.wait();
	pNf = pWI->pNf;
	delete pWI;
	return pNf.duplicate();
}


Notification* NotificationQueue::waitDequeueNotification(long milliseconds)
{
	Notification::Ptr pNf;
	WaitInfo* pWI = 0;
	{
		FastMutex::ScopedLock lock(_mutex);
		pNf = dequeueOne();
		if (pNf) return pNf.duplicate();
		pWI = new WaitInfo;
		_waitQueue.push_back(pWI);
	}
	if (pWI->nfAvailable.tryWait(milliseconds))
	{
		pNf = pWI->pNf;
	}
	else
	{
		FastMutex::ScopedLock lock(_mutex);
		pNf = pWI->pNf;
		for (WaitQueue::iterator it = _waitQueue.begin(); it != _waitQueue.end(); ++it)
		{
			if (*it == pWI)
			{
				_waitQueue.erase(it);
				break;
			}
		}
	}
	delete pWI;
	return pNf.duplicate();
}


void NotificationQueue::dispatch(NotificationCenter& notificationCenter)
{
	FastMutex::ScopedLock lock(_mutex);
	Notification::Ptr pNf = dequeueOne();
	while (pNf)
	{
		notificationCenter.postNotification(pNf);
		pNf = dequeueOne();
	}
}


void NotificationQueue::wakeUpAll()
{
	FastMutex::ScopedLock lock(_mutex);
	for (WaitQueue::iterator it = _waitQueue.begin(); it != _waitQueue.end(); ++it)
	{
		(*it)->nfAvailable.set();
	}
	_waitQueue.clear();
}


bool NotificationQueue::empty() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _nfQueue.empty();
}

	
int NotificationQueue::size() const
{
	FastMutex::ScopedLock lock(_mutex);
	return static_cast<int>(_nfQueue.size());
}


void NotificationQueue::clear()
{
	FastMutex::ScopedLock lock(_mutex);
	_nfQueue.clear();	
}


bool NotificationQueue::hasIdleThreads() const
{
	FastMutex::ScopedLock lock(_mutex);
	return !_waitQueue.empty();
}


Notification::Ptr NotificationQueue::dequeueOne()
{
	Notification::Ptr pNf;
	if (!_nfQueue.empty())
	{
		pNf = _nfQueue.front();
		_nfQueue.pop_front();
	}
	return pNf;
}


NotificationQueue& NotificationQueue::defaultQueue()
{
	static SingletonHolder<NotificationQueue> sh;
	return *sh.get();
}


} // namespace Poco
