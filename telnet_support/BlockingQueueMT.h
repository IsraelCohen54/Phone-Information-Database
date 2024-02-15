#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <class T>
class BlockingQueueMT
{
public:
	using value_type = T;

	BlockingQueueMT()
		: q()
		, m()
		, c()
	{}

	~BlockingQueueMT() = default;

	// Add an element to the queue.
	void Enqueue(const T &t)
	{
		{ // guard
			std::unique_lock<std::mutex> lock(m);
			q.push(t);
		}
		c.notify_one();
	}

	void Enqueue(T &&t)
	{
		{ // guard
			std::unique_lock<std::mutex> lock(m);
			q.push(std::move(t));
		}
		c.notify_one();
	}

  // Get the "front"-element.
  // If the queue is empty, wait till a element is avaiable.
	void Dequeue(T& a_poped)
	{
		std::unique_lock<std::mutex> lock(m);
		while(q.empty())
		{
			// release lock as long as the wait and reaquire it afterwards.
			c.wait(lock);
		}
		a_poped = std::move(q.front());
		q.pop();
	}

	bool TryDequeue(T& a_poped)
	{
		std::unique_lock<std::mutex> lock(m);
		if (q.empty())
		{
			return false;
		}
		a_poped = std::move(q.front());
		q.pop();
		return true;
	}

private:
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable c;
};