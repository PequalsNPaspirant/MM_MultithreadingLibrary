#pragma once

#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <cassert> //for assert()
#include <cmath>
#include <forward_list>
using namespace std;

/*
TODO:
This is Multi Producers Multi Consumers Unlimited Size Queue.
This is implemented using two mutex and one atomic variable. It uses spin lock.
It uses std::queue<T, Container<T>> to store data where Container is std::vector, std::list and std::forward_list
Producers never need to wait because its unlimited queue and will never be full.
Consumers have to wait if the queue is empty.
*/

namespace mm {

	//template <typename T, typename Container>
	template<typename T, template <typename... Args> class Container>
	class MultiProducersMultiConsumersUnlimitedQueue_v2
	{
		//Its empty class template. It supports only below two types.
	};

	template<typename T>
	class MultiProducersMultiConsumersUnlimitedQueue_v2<T, std::list>
	{
	public:
		MultiProducersMultiConsumersUnlimitedQueue_v2()
			: //head_{ queue_.begin() },
			//tail_{ queue_.end() },
			size_ {	0 }
		{}

		void push(T&& obj)
		{
			std::unique_lock<std::mutex> mlock(mutexProducer_);
			//if (size_.load() == 1)
			//{
			//	std::unique_lock<std::mutex> lock(mutexCommon_);
			//	//queue_.insert(queue_.end(), std::move(obj));
			//	queue_.push_back(std::move(obj));
			//	//if (size_.load() == 0)
			//	//	head_ = queue_.begin();
			//	//tail_ = queue_.end();
			//	//++tail_;
			//	++size_;
			//}
			//else
			{
				//queue_.insert(queue_.end(), std::move(obj));
				queue_.push_back(std::move(obj));
				//if (size_.load() == 0)
				//	head_ = queue_.begin();
				//tail_ = queue_.end();
				//++tail_;
				++size_;
			}

			//cout << "\nThread " << this_thread::get_id() << " pushed " << obj << " into queue. Queue size: " << queue_.size();
			
			mlock.unlock(); //release the lock on mutex, so that the notified thread can acquire that mutex immediately when awakened,
							//Otherwise waiting thread may try to acquire mutex before this thread releases it.
			cv_.notify_one(); //This will always notify one thread even though there are no waiting threads
		}

		//exception UNSAFE pop() version
		T pop()
		{
			std::unique_lock<std::mutex> mlock(mutexConsumer_);
			while (size_.load() == 0) //If the thread is active due to spurious wake-up or more number of threads are notified than the number of elements in queue, 
				//force it to check if queue is empty so that it can wait again if the queue is empty
			{
				cv_.wait(mlock);
			}
			//OR we can use below
			//cv_.wait(mlock, [this](){ return !this->queue_.empty(); });

			T obj;
			if (size_.load() == 1)
			{
				std::unique_lock<std::mutex> lock(mutexProducer_);
				//auto prevHead = head_;
				//++head_;
				obj = queue_.front();
				queue_.pop_front();
				//queue_.erase(queue_.begin());
				//head_ = queue_.begin();
				--size_;
			}
			else
			{
				--size_;
				//obj = *head_;
				//head_ = queue_.erase(head_);
				//head_ = queue_.begin();
				//auto prevHead = head_;
				//++head_;
				//obj = *prevHead;
				//queue_.erase(prevHead);
				obj = queue_.front();
				queue_.pop_front();
				//queue_.erase(queue_.begin());
			}
			
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
			return obj; //this object can be returned by copy/move and it will be lost if the copy/move constructor throws exception.
		}

		//exception SAFE pop() version
		void pop(T& outVal)
		{
			std::unique_lock<std::mutex> mlock(mutexConsumer_);
			while (queue_.empty())
			{
				cv_.wait(mlock);
			}
			outVal = queue_.front();
			queue_.pop_front();
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
		}

		//pop() with timeout. Returns false if timeout occurs.
		bool pop(T& outVal, const std::chrono::milliseconds& timeout)
		{
			std::unique_lock<std::mutex> mlock(mutexConsumer_);
			while (queue_.empty())
			{
				if(cv_.wait_for(mlock, timeout) == std::cv_status::timeout)
					return false;
			}
			//OR we can use below
			//cv_.wait_for(mlock, timeout, [this](){ return !this->queue_.empty(); });
			outVal = queue_.front();
			queue_.pop_front();
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
			return true;
		}

		size_t size()
		{
			return size_;
		}

		bool empty()
		{
			return size_ == 0;
		}

	private:
		std::list<T> queue_; //The queue internally uses the vector by default
		//typename std::list<T>::iterator head_;
		//typename std::list<T>::iterator tail_;
		std::mutex mutexProducer_;
		std::mutex mutexConsumer_;
		std::mutex mutexCommon_;
		std::condition_variable cv_;
		std::atomic<size_t> size_;
	};
















	template<typename T>
	class MultiProducersMultiConsumersUnlimitedQueue_v2<T, std::forward_list>
	{
	public:
		MultiProducersMultiConsumersUnlimitedQueue_v2()
			: last_{ queue_.before_begin() }, size_{ 0 }
		{}

		void push(T&& obj)
		{
			if (size_.load() == 1)
			{
				std::unique_lock<std::mutex> mlock(mutexCommon_);
				if (size_.load() == 0)
					last_ = queue_.before_begin();
				last_ = queue_.insert_after(last_, std::move(obj)); //Push element at the tail.
				++size_;
			}
			else
			{
				std::unique_lock<std::mutex> mlock(mutexProducer_);
				if (size_.load() == 0)
					last_ = queue_.before_begin();
				last_ = queue_.insert_after(last_, std::move(obj)); //Push element at the tail.
				++size_;

				//cout << "\nThread " << this_thread::get_id() << " pushed " << obj << " into queue. Queue size: " << queue_.size();
				mlock.unlock(); //release the lock on mutex, so that the notified thread can acquire that mutex immediately when awakened,
								//Otherwise waiting thread may try to acquire mutex before this thread releases it.
				cv_.notify_one(); //This will always notify one thread even though there are no waiting threads
			}
			
		}

		//exception UNSAFE pop() version
		T pop()
		{
			T obj;
			if (size_.load() == 1)
			{
				std::unique_lock<std::mutex> mlock(mutexCommon_);
				obj = queue_.front();
				//queue_.erase_after(queue_.before_begin());
				//if (queue_.empty())
				//	last_ = queue_.before_begin();
				queue_.pop_front();
				--size_;
			}
			else
			{
				std::unique_lock<std::mutex> mlock(mutexConsumer_);
				while (size_.load() == 0) //If the thread is active due to spurious wake-up or more number of threads are notified than the number of elements in queue, 
									   //force it to check if queue is empty so that it can wait again if the queue is empty
				{
					cv_.wait(mlock);
				}
				//OR we can use below
				//cond_.wait(mlock, [this](){ return !this->queue_.empty(); });

				--size_;
				obj = queue_.front();
				//queue_.erase_after(queue_.before_begin());
				queue_.pop_front();
				//if (queue_.empty())
				//	last_ = queue_.before_begin();
			}
			
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
			return obj; //this object can be returned by copy/move and it will be lost if the copy/move constructor throws exception.
		}

		//exception SAFE pop() version
		void pop(T& outVal)
		{
			std::unique_lock<std::mutex> mlock(mutexConsumer_);
			while (queue_.empty())
			{
				cv_.wait(mlock);
			}
			outVal = queue_.front();
			queue_.erase_after(queue_.before_begin());
			--size_;
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
		}

		//pop() with timeout. Returns false if timeout occurs.
		bool pop(T& outVal, const std::chrono::milliseconds& timeout)
		{
			std::unique_lock<std::mutex> mlock(mutexConsumer_);
			while (queue_.empty())
			{
				if (cv_.wait_for(mlock, timeout) == std::cv_status::timeout)
					return false;
			}
			//OR we can use below
			//cv_.wait_for(mlock, timeout, [this](){ return !this->queue_.empty(); });
			outVal = queue_.front();
			queue_.erase_after(queue_.before_begin());
			--size_;
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
			return true;
		}

		size_t size()
		{
			return std::distance(queue_.begin(), queue_.end());
		}

		bool empty()
		{
			return queue_.empty();
		}

	private:
		std::forward_list<T> queue_; //The queue internally uses the vector by default
		typename std::forward_list<T>::iterator last_;
		std::atomic<size_t> size_;
		std::mutex mutexProducer_;
		std::mutex mutexConsumer_;
		std::mutex mutexCommon_;
		std::condition_variable cv_;
	};

	template <typename... Args> class Undefined {};

	template<typename T>
	class MultiProducersMultiConsumersUnlimitedQueue_v2<T, Undefined>
	{
	private:
		struct Node
		{
			Node(T obj)
				: data_{ std::move(obj) }, next_{ nullptr }
			{
			}
			T data_;
			Node* next_;
		};

		class ForwardList
		{
		public:
			ForwardList()
				: head_{nullptr},
				tail_{nullptr}
			{}

			~ForwardList()
			{
				if (head_ == nullptr)
					return;

				Node* curr = head_;
				do
				{
					Node* removed = curr;
					curr = curr->next_;
					delete removed;
				} while (curr != tail_);
			}

			void pop_front(T& outVal)
			{
				outVal = head_->data_;
				Node* removed = head_;
				head_ = head_->next_;
				if (head_ == nullptr)
					tail_ = nullptr;
				delete removed;
			}

			void push_back(T obj)
			{
				Node* pn = new Node(std::move(obj));
				if (tail_ == nullptr)
					head_ = pn;
				else
					tail_->next_ = pn;

				tail_ = pn;
			}

		private:
			Node* head_;
			Node* tail_;
		};

	public:
		MultiProducersMultiConsumersUnlimitedQueue_v2()
			: noAtomicSize_{ 0 }
		{}

		void push(T&& obj)
		{
			std::unique_lock<std::mutex> p_lock(mutexProducer_);
			queue_.push_back(std::move(obj)); //Push element at the tail.
			++noAtomicSize_;

			//cout << "\nThread " << this_thread::get_id() << " pushed " << obj << " into queue. Queue size: " << queue_.size();
			p_lock.unlock(); //release the lock on mutex, so that the notified thread can acquire that mutex immediately when awakened,
							//Otherwise waiting thread may try to acquire mutex before this thread releases it.
			cv_.notify_one(); //This will always notify one thread even though there are no waiting threads
		}

		//exception UNSAFE pop() version
		T pop()
		{
			T obj;
			std::unique_lock<std::mutex> c_lock(mutexConsumer_);
			std::unique_lock<std::mutex> p_lock(mutexProducer_);
			if (noAtomicSize_ <= 1) 
			{
				//If the thread is active due to spurious wake-up or more number of threads are notified than the number of elements in queue, 
				//force it to check if queue is empty so that it can wait again if the queue is empty
				while (noAtomicSize_ == 0)
				{
					cv_.wait(p_lock);
					//OR we can use below
					//cond_.wait(mlock, [this](){ return !this->queue_.empty(); });
				}

				queue_.pop_front(obj);
				--noAtomicSize_;
			}
			else
			{
				--noAtomicSize_;
				p_lock.unlock(); //Release the lock as this consumer thread is working on a part of queue which will not be touched by any producer thread because size > 1
				
				queue_.pop_front(obj);
			}

			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
			return obj; //this object can be returned by copy/move and it will be lost if the copy/move constructor throws exception.
		}

		//exception SAFE pop() with timeout. Returns false if timeout occurs.
		bool pop(T& outVal, const std::chrono::milliseconds& timeout)
		{
			std::unique_lock<std::mutex> c_lock(mutexConsumer_);

			std::unique_lock<std::mutex> p_lock(mutexProducer_);
			//If the thread is active due to spurious wake-up or more number of threads are notified than the number of elements in queue, 
			//force it to check if queue is empty so that it can wait again if the queue is empty
			while (noAtomicSize_ == 0)
			{
				//cv_.wait(p_lock);
				if (cv_.wait_for(p_lock, timeout) == std::cv_status::timeout)
					return false;
			}
			//OR
			//cond_.wait(mlock, [this](){ return !this->queue_.empty(); });
			//cv_.wait_for(mlock, timeout, [this](){ return !this->queue_.empty(); });

			

			if (noAtomicSize_ == 1)
			{
				--noAtomicSize_;
			}
			else
			{
				--noAtomicSize_;
				p_lock.unlock(); //Release the lock as this consumer thread is working on a part of queue which will not be touched by any producer thread because size > 1
			}

			queue_.pop_front(outVal);
			//cout << "\nThread " << this_thread::get_id() << " popped " << obj << " from queue. Queue size: " << queue_.size();
			return true;
		}

		size_t size()
		{
			std::unique_lock<std::mutex> p_lock(mutexProducer_);
			return noAtomicSize_;
		}

		bool empty()
		{
			std::unique_lock<std::mutex> p_lock(mutexProducer_);
			return noAtomicSize_ == 0;
		}

	private:
		ForwardList queue_;
		//std::atomic<size_t> size_;
		size_t noAtomicSize_;
		std::mutex mutexProducer_;
		std::mutex mutexConsumer_;
		std::mutex mutexCommon_;
		std::condition_variable cv_;
	};
}