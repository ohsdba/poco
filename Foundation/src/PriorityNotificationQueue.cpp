//
// PriorityNotificationQueue.cpp
//
// $Id: //poco/Main/Foundation/src/PriorityNotificationQueue.cpp#1 $
//
// Library: Foundation
// Package: Notifications
// Module:  PriorityNotificationQueue
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
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


#include "Poco/PriorityNotificationQueue.h"
#include "Poco/NotificationCenter.h"
#include "Poco/Notification.h"
#include "Poco/SingletonHolder.h"


namespace Poco {


PriorityNotificationQueue::PriorityNotificationQueue()
{
}


PriorityNotificationQueue::~PriorityNotificationQueue()
{
	clear();
}


void PriorityNotificationQueue::enqueueNotification(Notification::Ptr pNotification, int priority)
{
	poco_check_ptr (pNotification);
	FastMutex::ScopedLock lock(_mutex);
	if (_waitQueue.empty())
	{
		_nfQueue.insert(NfQueue::value_type(priority, pNotification));
	}
	else
	{
		poco_assert_dbg(_nfQueue.empty());
		WaitInfo* pWI = _waitQueue.front();
		_waitQueue.pop_front();
		pWI->pNf = pNotification;
		pWI->nfAvailable.set();
	}	
}


Notification* PriorityNotificationQueue::dequeueNotification()
{
	FastMutex::ScopedLock lock(_mutex);
	return dequeueOne().duplicate();
}


Notification* PriorityNotificationQueue::waitDequeueNotification()
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


Notification* PriorityNotificationQueue::waitDequeueNotification(long milliseconds)
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


void PriorityNotificationQueue::dispatch(NotificationCenter& notificationCenter)
{
	FastMutex::ScopedLock lock(_mutex);
	Notification::Ptr pNf = dequeueOne();
	while (pNf)
	{
		notificationCenter.postNotification(pNf);
		pNf = dequeueOne();
	}
}


void PriorityNotificationQueue::wakeUpAll()
{
	FastMutex::ScopedLock lock(_mutex);
	for (WaitQueue::iterator it = _waitQueue.begin(); it != _waitQueue.end(); ++it)
	{
		(*it)->nfAvailable.set();
	}
	_waitQueue.clear();
}


bool PriorityNotificationQueue::empty() const
{
	FastMutex::ScopedLock lock(_mutex);
	return _nfQueue.empty();
}

	
int PriorityNotificationQueue::size() const
{
	FastMutex::ScopedLock lock(_mutex);
	return static_cast<int>(_nfQueue.size());
}


void PriorityNotificationQueue::clear()
{
	FastMutex::ScopedLock lock(_mutex);
	_nfQueue.clear();	
}


bool PriorityNotificationQueue::hasIdleThreads() const
{
	FastMutex::ScopedLock lock(_mutex);
	return !_waitQueue.empty();
}


Notification::Ptr PriorityNotificationQueue::dequeueOne()
{
	Notification::Ptr pNf;
	NfQueue::iterator it = _nfQueue.begin();
	if (it != _nfQueue.end())
	{
		pNf = it->second;
		_nfQueue.erase(it);
	}
	return pNf;
}


PriorityNotificationQueue& PriorityNotificationQueue::defaultQueue()
{
	static SingletonHolder<PriorityNotificationQueue> sh;
	return *sh.get();
}


} // namespace Poco
