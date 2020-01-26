#include <iostream>
#include <random>
#include <chrono>
#include <locale>
#include <memory>
#include <vector>
#include <list>
#include <forward_list>
using namespace std;

#include "MultiProducersMultiConsumersUnsafeQueue_v1.h"
#include "MultiProducersMultiConsumersUnlimitedQueue_v1.h"
#include "MultiProducersMultiConsumersUnlimitedLockFreeQueue_v1.h"
#include "MultiProducersMultiConsumersUnlimitedLockFreeQueue_v2.h"

#include "MultiProducersMultiConsumersFixedSizeQueue_v1.h"
#include "MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx.h"

#include "MM_UnitTestFramework/MM_UnitTestFramework.h"

#define USE_SLEEP

namespace mm {

	std::random_device rd;
	std::mt19937 mt32(rd()); //for 32 bit system
	std::mt19937_64 mt64(rd()); //for 64 bit system
	std::uniform_int_distribution<size_t> dist(0, 99);
	vector<size_t> sleepTimesNanosVec;
	size_t totalSleepTimeNanos;
	/*
	Results in the tabular format like below: (where (a,b,c) = a producers b consumers and c operations by each thread)
	(#producdrs, #consumers, #operations)          Queue1       Queue2
	(1,1,1)        
	(1,1,10)        
	(2,2,2)
	*/

	struct ResultSet
	{
		int numProducers_;
		int numConsumers_;
		int numOperations_;
		int queueSize_;
		std::vector<size_t> nanosPerQueueType_;
	};

	std::vector<ResultSet> results;

	template<typename T>
	void producerThreadFunction(T& queue, size_t numOperationsPerThread, int threadId)
	{
		for (int i = 0; i < numOperationsPerThread; ++i)
		{
#ifdef USE_SLEEP
			//int sleepTime = dist(mt64) % 100;
			size_t sleepTime = sleepTimesNanosVec[threadId * numOperationsPerThread + i];
			this_thread::sleep_for(chrono::nanoseconds(sleepTime));
#endif
			//int n = dist(mt64);
			//cout << "\nThread " << this_thread::get_id() << " pushing " << n << " into queue";
			int n = i;
			queue.push(std::move(n));
		}
	}

	template<>
	void producerThreadFunction<MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx<int>>(MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx<int>& queue, size_t numOperationsPerThread, int threadId)
	{
		for (int i = 0; i < numOperationsPerThread; ++i)
		{
#ifdef USE_SLEEP
			size_t sleepTime = sleepTimesNanosVec[threadId * numOperationsPerThread + i];
			this_thread::sleep_for(chrono::nanoseconds(sleepTime));
#endif
			int n = i;
			queue.push(std::move(n), threadId);
		}
	}

	template<typename T>
	void consumerThreadFunction(T& queue, size_t numOperationsPerThread, int threadId)
	{
		for(int i = 0; i < numOperationsPerThread; ++i)
		{
#ifdef USE_SLEEP
			//int sleepTime = dist(mt64) % 100;
			size_t sleepTime = sleepTimesNanosVec[threadId * numOperationsPerThread + i];
			this_thread::sleep_for(chrono::nanoseconds(sleepTime));
#endif
			int n = queue.pop();
			//cout << "\nThread " << this_thread::get_id() << " popped " << n << " from queue";
		}
	}
	template<>
	void consumerThreadFunction<MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx<int>>(MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx<int>& queue, size_t numOperationsPerThread, int threadId)
	{
		for (int i = 0; i < numOperationsPerThread; ++i)
		{
#ifdef USE_SLEEP
			int sleepTime = sleepTimesNanosVec[threadId * numOperationsPerThread + i];
			this_thread::sleep_for(chrono::nanoseconds(sleepTime));
#endif
			int n = queue.pop(threadId);
		}
	}

	struct separate_thousands : std::numpunct<char> {
		char_type do_thousands_sep() const override { return ','; }  // separate with commas
		string_type do_grouping() const override { return "\3"; } // groups of 3 digit
	};

	template<typename Tqueue>
	void test_mpmcu_queue(Tqueue& queue, size_t numProducerThreads, size_t numConsumerThreads, size_t numOperationsPerThread, int resultIndex)
	{
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		//Tqueue queue = createQueue_sfinae<Tqueue>(queueSize);
		const size_t threadsCount = numProducerThreads > numConsumerThreads ? numProducerThreads : numConsumerThreads;
		vector<std::thread> producerThreads;
		vector<std::thread> consumerThreads;
		int threadId = -1;
		for (size_t i = 0; i < threadsCount; ++i)
		{
			if(i < numProducerThreads)
				producerThreads.push_back(std::thread(producerThreadFunction<Tqueue>, std::ref(queue), numOperationsPerThread, ++threadId));
			if (i < numConsumerThreads)
				consumerThreads.push_back(std::thread(consumerThreadFunction<Tqueue>, std::ref(queue), numOperationsPerThread, ++threadId));
		}

		for (size_t i = 0; i < numProducerThreads; ++i)
			producerThreads[i].join();

		for (size_t i = 0; i < numConsumerThreads; ++i)
			consumerThreads[i].join();

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		unsigned long long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

		//int number = 123'456'789;
		//std::cout << "\ndefault locale: " << number;
		auto thousands = std::make_unique<separate_thousands>();
		std::cout.imbue(std::locale(std::cout.getloc(), thousands.release()));
		//std::cout << "\nlocale with modified thousands: " << number;
		cout << "      Queue is empty ? : " << (queue.empty() ? "Yes" : "No") << " Queue Size : " << queue.size();
		cout << "\n    Total Duration: " << duration << " nanos."
			<< " Effective Duration: " << duration - (totalSleepTimeNanos / (numProducerThreads + numConsumerThreads)) << " nanos."
			<< " Total sleep time: " << totalSleepTimeNanos << " nanos." 
			<< " Average sleep time per thread: " << totalSleepTimeNanos / (numProducerThreads + numConsumerThreads) << " nanos.";
		results[resultIndex].nanosPerQueueType_.push_back(duration);
	}

	template<typename Tqueue>
	void test_mpmcu_queue_sfinae(size_t numProducerThreads, size_t numConsumerThreads, size_t numOperationsPerThread, size_t queueSize, int resultIndex)
	{
		Tqueue queue{};
		test_mpmcu_queue(queue, numProducerThreads, numConsumerThreads, numOperationsPerThread, resultIndex);
	}
	template<>
	void test_mpmcu_queue_sfinae<MultiProducersMultiConsumersFixedSizeQueue_v1<int>>(size_t numProducerThreads, size_t numConsumerThreads, size_t numOperationsPerThread, size_t queueSize, int resultIndex)
	{
		MultiProducersMultiConsumersFixedSizeQueue_v1<int> queue{ queueSize };
		test_mpmcu_queue(queue, numProducerThreads, numConsumerThreads, numOperationsPerThread, resultIndex);
	}
	template<>
	void test_mpmcu_queue_sfinae<MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx<int>>(size_t numProducerThreads, size_t numConsumerThreads, size_t numOperationsPerThread, size_t queueSize, int resultIndex)
	{
		MultiProducersMultiConsumersFixedSizeLockFreeQueue_vx<int> queue{ queueSize, numProducerThreads, numConsumerThreads };
		test_mpmcu_queue(queue, numProducerThreads, numConsumerThreads, numOperationsPerThread, resultIndex);
	}
	
	void runTestCase(int numProducerThreads, int numConsumerThreads, int numOperationsPerThread, int queueSize, int resultIndex)
	{
		//size_t  = 25;
		//size_t  = 25;
		//size_t  = 50;
		//size_t  = 8; // numProducerThreads * numOperationsPerThread / 10;

#ifdef USE_SLEEP
		size_t total = (numProducerThreads + numConsumerThreads) * numOperationsPerThread;
		sleepTimesNanosVec.reserve(total);
		totalSleepTimeNanos = 0;
		for (size_t i = 0; i < total; ++i)
		{
			size_t sleepTime = dist(mt64); // % 10000;
			totalSleepTimeNanos += sleepTime;
			sleepTimesNanosVec.push_back(sleepTime);
		}
		//totalSleepTimeNanos *= 1000ULL;
#endif

		/***** Unlimited Queues ****/
		//The below queue crashes the program due to lack of synchronization
		//test_mpmcu_queue_sfinae<UnsafeQueue_v1<int>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex); 
		//test_mpmcu_queue_sfinae<MultiProducersMultiConsumersUnlimitedQueue_v1<int, std::vector<int>>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex);
		//test_mpmcu_queue_sfinae<MultiProducersMultiConsumersUnlimitedQueue_v1<int, std::vector>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex);
		cout << "\nMPMC-U-v1-deque"; test_mpmcu_queue_sfinae<MultiProducersMultiConsumersUnlimitedQueue_v1<int>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex);
		cout << "\nMPMC-U-v1-list"; test_mpmcu_queue_sfinae<MultiProducersMultiConsumersUnlimitedQueue_v1<int, std::list>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex);
		cout << "\nMPMC-U-v1-fwlist"; test_mpmcu_queue_sfinae<MultiProducersMultiConsumersUnlimitedQueue_v1<int, std::forward_list>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex);
		cout << "\nMPMC-U-LF-v1"; test_mpmcu_queue_sfinae<MultiProducersMultiConsumersUnlimitedLockFreeQueue_v1<int>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, 0, resultIndex);

		/***** Fixed Size Queues ****/
		cout << "\nMPMC-FS-v1"; test_mpmcu_queue_sfinae<MultiProducersMultiConsumersFixedSizeQueue_v1<int>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, queueSize, resultIndex);
		//The below queue does not work
		//test_mpmcu_queue_sfinae<MultiProducersMultiConsumersFixedSizeLockFreeQueue_v1<int>>(numProducerThreads, numConsumerThreads, numOperationsPerThread, queueSize, resultIndex);
	}

	MM_DECLARE_FLAG(Multithreading_mpmcu_queue);
	MM_UNIT_TEST(Multithreading_mpmcu_queue_test, Multithreading_mpmcu_queue)
	{
		MM_SET_PAUSE_ON_ERROR(true);

		results =
		{
		{ 1, 1, 1, 1, {} },
		{ 1, 1, 100, 10, {} },
		{ 2, 2, 100, 10, {} },
		{ 3, 3, 100, 10, {} },
		{ 4, 4, 100, 10, {} },
		{ 4, 4, 500, 10, {} },
		{ 5, 5, 100, 10, {} },
		{ 6, 6, 100, 10, {} },
		{ 7, 7, 100, 10, {} },
		{ 8, 8, 100, 10, {} },
		{ 9, 9, 100, 10, {} },
		{ 10, 10, 100, 10, {} },
		{ 10, 10, 500, 10, {} },
		{ 20, 20, 5000, 10,{} },
		{ 100, 100, 100000, 10,{} },
		};

		for (int i = 0; i < results.size(); ++i)
		{
			cout << "\n===========Test case no.: " << i 
				<< "   Producers: " << results[i].numProducers_ 
				<< "   Consumers: " << results[i].numConsumers_ 
				<< "   numOperationsPerThread: " << results[i].numOperations_
				<< "   Queue size: " << results[i].queueSize_;
			runTestCase(results[i].numProducers_, results[i].numConsumers_, results[i].numOperations_, results[i].queueSize_, i);
		}

		//Print results
		constexpr const int subCol1 = 5;
		constexpr const int subCol2 = 5;
		constexpr const int subCol3 = 9;
		constexpr const int subCol4 = 5;
		constexpr const int firstColWidth = subCol1 + subCol2 + subCol3 + subCol4;
		constexpr const int colWidth = 20;
		cout
			<< "\n\n"
			<< std::setw(firstColWidth) << "Test Case"
			<< std::setw(colWidth) << "MPMC-U-v1-deque"
			<< std::setw(colWidth) << "MPMC-U-v1-list"
			<< std::setw(colWidth) << "MPMC-U-v1-fwlist"
			<< std::setw(colWidth) << "MPMC-U-LF-v1"
			<< std::setw(colWidth) << "MPMC-FS-v1"
			<< std::setw(colWidth) << "MPMC-FS-LF-v1";
		cout << "\n"
			<< std::setw(subCol1) << "Ps"
			<< std::setw(subCol2) << "Cs"
			<< std::setw(subCol3) << "Ops"
			<< std::setw(subCol4) << "Qsz";

		cout << "\n";
		for (int i = 0; i < results.size(); ++i)
		{
			cout << "\n"
				<< std::setw(subCol1) << results[i].numProducers_
				<< std::setw(subCol2) << results[i].numConsumers_
				<< std::setw(subCol3) << results[i].numOperations_
				<< std::setw(subCol4) << results[i].queueSize_;
			for (int k = 0; k < results[i].nanosPerQueueType_.size(); ++k)
				cout << std::setw(colWidth) << results[i].nanosPerQueueType_[k];
		}
	}
}

/*
Results:
without sleep time:

              Test Case     MPMC-U-v1-deque      MPMC-U-v1-list    MPMC-U-v1-fwlist        MPMC-U-LF-v1          MPMC-FS-v1       MPMC-FS-LF-v1
  Ps   Cs      Ops  Qsz

  1    1        1    1          10,512,800           5,097,400           5,619,400           4,938,300           4,758,900
  1    1      100   10           3,764,800           5,850,400           9,088,200           3,342,600           3,662,100
  2    2      100   10           6,814,900           6,649,000           7,908,300           8,334,500           6,703,300
  3    3      100   10          10,558,200          10,851,900          11,768,000          38,614,000           8,925,800
  4    4      100   10          15,684,400          17,696,200          16,985,400          14,359,100          12,815,600
  4    4      500   10          15,503,200          19,127,400          15,370,500          20,446,600          12,831,500
  5    5      100   10          14,342,200          14,654,900          16,962,500          13,110,000          13,066,800
  6    6      100   10          15,550,000          14,968,800          15,055,900          15,713,100          15,805,800
  7    7      100   10          18,838,900          17,118,400          27,866,400          32,619,300          46,323,900
  8    8      100   10          34,667,600          38,316,500          40,968,200          24,744,700          33,581,400
  9    9      100   10          46,101,800          44,937,800          22,793,300          54,987,200          32,195,900
 10   10      100   10          40,243,600          32,460,100          25,783,200          24,804,900          30,366,400
 10   10      500   10          38,266,300          22,844,900          21,782,700          20,834,000          22,432,700
 20   20    5,000   10          55,132,200          72,395,200          67,074,900          59,002,600          81,626,200
100  100  100,000   10       2,295,550,100       2,431,142,400       2,478,145,000       1,805,269,200       5,109,587,200

with sleep time:

             Test Case     MPMC-U-v1-deque      MPMC-U-v1-list    MPMC-U-v1-fwlist        MPMC-U-LF-v1          MPMC-FS-v1       MPMC-FS-LF-v1
 Ps   Cs      Ops  Qsz
  
  1    1        1    1          92,102,700          10,272,600           6,010,200           3,792,700           6,103,700
  1    1      100   10          83,918,700         100,351,700          53,518,900          19,444,000          86,004,600
  2    2      100   10          34,781,400          92,310,700          63,236,300          95,707,000          23,135,100
  3    3      100   10          21,879,600          32,206,500          48,652,900          21,996,700          40,132,600
  4    4      100   10          56,095,700          44,570,000          80,877,500          99,548,200          83,371,200
  4    4      500   10         185,091,700         157,041,800         359,332,200          48,801,400          57,415,300
  5    5      100   10          36,853,100          33,966,900          35,237,300          58,058,000          37,894,300
  6    6      100   10          48,523,400          53,955,700          57,150,600          49,072,000          62,781,500
  7    7      100   10          72,806,100          74,893,300          53,730,900          50,012,300          51,488,200
  8    8      100   10          45,376,600          54,049,900         101,916,200          72,101,200          67,888,400
  9    9      100   10          48,577,500          95,669,300         108,464,700         119,415,700         127,087,700
 10   10      100   10          89,290,600         105,511,700          80,415,200          94,005,500          53,200,700
 10   10      500   10         127,712,600         118,717,600         128,169,700         103,525,000          96,378,100
 20   20    5,000   10       1,106,625,400       1,019,265,200       1,241,381,200         854,424,200       1,328,750,700
100  100  100,000   10      51,701,202,200      60,164,323,300      57,437,982,400      53,855,787,000      63,660,670,200


*/