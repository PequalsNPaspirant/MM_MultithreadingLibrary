#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <cassert> //for assert()
#include <cmath>
using namespace std;

//#include "Multithreading\Multithreading_SingleProducerMultipleConsumers_v1.h"
#include "MM_UnitTestFramework/MM_UnitTestFramework.h"

/*
This is Multi Producers Multi Consumers Fixed Size Queue.
This is very common and basic implemention using one mutex and two condition variables (one each for producers and consumers).
It uses std::vector of fixed length to store data.
Producers have to wait if the queue is full.
Consumers have to wait if the queue is empty.
*/

namespace mm {

	template <typename T>
	class MultiProducersMultiConsumersFixedSizeQueue_v1
	{
	public:
		MultiProducersMultiConsumersFixedSizeQueue_v1(size_t maxSize)
			: maxSize_(maxSize), 
			vec_(maxSize), 
			//size_(0), 
			head_(0), 
			tail_(0)
		{
		}

		bool push(T&& obj, const std::chrono::milliseconds& timeout = std::chrono::milliseconds{1000 * 60 * 60}) //default timeout = 1 hr
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			//while(size_ == maxSize_)
			while (head_ - tail_ == maxSize_)
			{
				//cvProducers_.wait(mlock);
				if(cvProducers_.wait_for(mlock, timeout) == std::cv_status::timeout)
					return false;
			}
			vec_[head_ % maxSize_] = std::move(obj);
			++head_;
			//++size_;
			//cout << "\nThread " << this_thread::get_id() << " pushed " << obj << " into queue. Queue size: " << size_;
			mlock.unlock(); //release the lock on mutex, so that the notified thread can acquire that mutex immediately when awakened,
							//Otherwise waiting thread may try to acquire mutex before this thread releases it.
			cvConsumers_.notify_one();
			return true;
		}

		//pop() with timeout. Returns false if timeout occurs.
		bool pop(T& outVal, const std::chrono::milliseconds& timeout)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			//while (size_ == 0)
			while (head_ == tail_)
			{
				//cvConsumers_.wait(mlock);
				if (cvConsumers_.wait_for(mlock, timeout) == std::cv_status::timeout)
					return false;
			}
			//OR
			//cond_.wait(mlock, [this](){ return this->size_ != 0; });
			//cond_.wait_for(mlock, timeout, [this](){ return this->size_ != 0; });

			outVal = std::move(vec_[tail_ % maxSize_]);
			if (++tail_ == maxSize_)
			{
				head_ %= maxSize_;
				tail_ %= maxSize_;
			}
			//--size_;
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << size_;

			mlock.unlock();
			cvProducers_.notify_one();
			return true;
		}

		size_t size()
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			//return size_;
			//return head_ >= tail_ ? head_ - tail_ : maxSize_ - (tail_ - head_);
			return head_ - tail_;
		}

		bool empty()
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			//return vec_.empty(); //vector is never empty. The elements will be overwritten by push if the queue is already full.
			//return size_ == 0;
			return head_ == tail_;
		}

	private:
		size_t maxSize_;
		std::vector<T> vec_; //This will be used as ring buffer / circular queue
		//size_t size_;
		size_t head_; //stores the index where next element will be pushed
		size_t tail_; //stores the index of object which will be popped
		std::mutex mutex_;
		std::condition_variable cvProducers_;
		std::condition_variable cvConsumers_;
	};
	
}