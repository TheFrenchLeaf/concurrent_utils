/*
 * BlockingAccessor.hpp
 *
 *  Created on: 22 nov. 2011
 *      Author: Guillaume Chatelet
 */

#ifndef BLOCKINGACCESSOR_HPP_
#define BLOCKINGACCESSOR_HPP_

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cassert>
#include <stdexcept>

namespace concurrent {

/**
 * Exception thrown by BlockingAccessor when terminate flag is set
 */
struct terminated : public std::exception {};

/**
 * Thread safe access to a T object
 *
 * By setting terminate to true, each access to this slot will generate a terminated exception
 */
template<typename T>
struct BlockingAccessor : private boost::noncopyable {
    BlockingAccessor(const T&object) : m_SharedTerminate(false) {
        internal_set(object);
    }

    void set(const T& object) {
        // locking the shared object
        ::boost::mutex::scoped_lock lock(m_Mutex);
        internal_set(object);
        lock.unlock();
        // notifying shared structure is updated
        m_Condition.notify_one();
    }

    void terminate(bool value = true) {
        ::boost::unique_lock<boost::mutex> lock(m_TerminateMutex);
        m_SharedTerminate = value;
        lock.unlock();
        m_Condition.notify_all();
    }

    void waitGet(T& value) {
        ::boost::unique_lock<boost::mutex> lock(m_Mutex);

        checkTermination();

        // blocking until at least one element in the list
        while (!m_SharedObjectSet) {
            m_Condition.wait(lock);
            checkTermination();
        }

        internal_unset(value);
    }

    bool tryGet(T& holder) {
        // locking the shared object
        ::boost::lock_guard<boost::mutex> lock(m_Mutex);
        checkTermination();

        if (!m_SharedObjectSet)
            return false;

        internal_unset(holder);
        return true;
    }
private:
    inline void checkTermination() const {
        ::boost::lock_guard<boost::mutex> lock(m_TerminateMutex);
        if (m_SharedTerminate)
            throw terminated();
    }

    inline void internal_set(const T& value){
        m_SharedObject = value;
        m_SharedObjectSet = true;
    }

    inline void internal_unset(T& value){
        assert(m_SharedObjectSet);
        value = m_SharedObject;
        m_SharedObjectSet = false;
    }

    mutable ::boost::mutex m_Mutex;
    ::boost::condition_variable m_Condition;
    mutable ::boost::mutex m_TerminateMutex;
    bool m_SharedTerminate;
    T m_SharedObject;
    bool m_SharedObjectSet;
};

}  // namespace concurrent

#endif /* BLOCKINGACCESSOR_HPP_ */
