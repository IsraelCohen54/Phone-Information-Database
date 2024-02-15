#pragma once
#include <array>
#include <cassert>
#include <mutex>
#include <condition_variable>

namespace experis
{
static constexpr short INC = 1;

template <typename T, size_t MaxQueue>
class CyclicBlockingQueue
{
public:
	CyclicBlockingQueue()
	: m_circularArrayQueue{}
	, m_indPush{0}
	, m_indPop{0}
	, m_maxSizeQueue{MaxQueue + INC}
	{
		assert(MaxQueue > 0);
	}
	CyclicBlockingQueue(const CyclicBlockingQueue& a_other) = delete;
	CyclicBlockingQueue& operator=(const CyclicBlockingQueue& a_other) = delete;
	~CyclicBlockingQueue() = default;

	void Enqueue(T a_val)
	{
		{
			std::unique_lock<std::mutex> lockUntilNotFull(m_arrayMutex);
			
			while ((m_indPush + INC) % m_maxSizeQueue == m_indPop) // => Array currently Full
			{		   
				m_popEvent.wait(lockUntilNotFull);
			}

			m_circularArrayQueue.at(m_indPush) = a_val;

			m_indPush = (m_indPush + INC) % m_maxSizeQueue;
		}
		m_popWaitVacantArray.notify_all();
	}

	void Dequeue(T& a_valContainer)
	{
		std::unique_lock<std::mutex> lockUntilhasVal(m_arrayMutex);
		{
			while(m_indPop == m_indPush)
			{
				m_popWaitVacantArray.wait(lockUntilhasVal);
			}
			a_valContainer = m_circularArrayQueue.at(m_indPop); // OS might throw exception and would catch it (cctor no memory), and get out of Dequeue func, but value won't be deleted.
															 // after expection + catch, it would return to the main program (each thread has it's own callstack)
			m_indPop = (m_indPop + INC) % m_maxSizeQueue;
		}
		m_popEvent.notify_all();
	}

	bool IsVacant_not_mt()
	{
		// Return val happen, exist from func, than dtor, than empty() answer can be changed... not in the midlle
		std::unique_lock<std::mutex> lockUntilAfterReturnVal(m_arrayMutex);
		{
			return (m_indPop == m_indPush);
		}
	}

	bool IsFull_not_mt()
	{
		// Return val happen, exist from func, than dtor, than IsFull() answer can be changed... not in the midlle
		std::unique_lock<std::mutex> lockUntilAfterReturnVal(m_arrayMutex);
		{
			return ((m_indPush + INC) % m_maxSizeQueue == m_indPop);
		}
	}
	
private:
	size_t m_indPush;
	size_t m_indPop;
	size_t m_maxSizeQueue = MaxQueue + INC;
	std::array<T, MaxQueue + INC> m_circularArrayQueue;

	std::mutex m_arrayMutex;

	std::condition_variable m_popWaitVacantArray;
	std::condition_variable m_popEvent;
};

} // namespace experis