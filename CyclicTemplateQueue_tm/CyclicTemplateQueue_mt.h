#pragma once
#include <array>
#include <cassert>
#include <mutex>
#include <condition_variable>

namespace experis
{
static constexpr short INC = 1;

template <typename T, size_t MAX_QUEUE>
class CyclicBlockingQueue_mt
{
public:
	CyclicBlockingQueue_mt()
	: m_array{}
	, m_indPush{0}
	, m_indPop{0}
	, m_maxSizeQueue{MAX_QUEUE + 1}
	{
		assert(MAX_QUEUE > 0);
	}
	CyclicBlockingQueue_mt(const CyclicBlockingQueue_mt& a_other) = delete;
	CyclicBlockingQueue_mt& operator=(const CyclicBlockingQueue_mt& a_other) = delete;
	~CyclicBlockingQueue_mt() = default;

	void Push(T a_val)
	{
		{
			std::unique_lock<std::mutex> lockUntilNotFull(m_lock);
			
			while ((m_indPush + 1) % m_maxSizeQueue == m_indPop) // => Array currently Full
			{		   
				m_popEvent.wait(lockUntilNotFull);
			}

			m_array.at(m_indPush) = a_val;

			m_indPush = (m_indPush + INC) % m_maxSizeQueue;
		}
		m_popWaitVacantArray.notify_all();
	}

	void Pop(T& a_valContainer)
	{
		std::unique_lock<std::mutex> lockUntilhasVal(m_lock);
		{
			while(m_indPop == m_indPush)
			{
				m_popWaitVacantArray.wait(lockUntilhasVal);
			}
			a_valContainer = m_array.at(m_indPop); // OS might throw exception and would catch it (cctor no memory), and get out of Pop func, but value won't be deleted.
															 // after expection + catch, it would return to the main program (each thread has it's own callstack)
			m_indPop = (m_indPop + INC) % m_maxSizeQueue;
		}
		m_popEvent.notify_all();
	}
	
private:
	std::array<T, MAX_QUEUE + 1> m_array;
	size_t m_indPush;
	size_t m_indPop;
	size_t m_maxSizeQueue = MAX_QUEUE + 1;

	std::mutex m_lock;
	std::condition_variable m_popWaitVacantArray;
	std::condition_variable m_popEvent;
};

} // namespace experis