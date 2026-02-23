#pragma once

#include <memory>

template<typename T> class AutoPtr
{
	public:
		explicit AutoPtr (T* p = nullptr) :
			p_(p)
		{
		}

		~AutoPtr () = default;

		AutoPtr (const AutoPtr&) = delete;
		AutoPtr& operator = (const AutoPtr&) = delete;

		void deallocate ()
		{
			p_.reset();
		}

		T* release ()
		{
			return p_.release();
		}

		void operator = (T* p)
		{
			p_.reset(p);
		}

		T* operator -> () const
		{
			return p_.get();
		}

		T& operator * () const
		{
			return *p_;
		}

		explicit operator bool () const
		{
			return p_ != nullptr;
		}

		operator T* () const
		{
			return p_.get();
		}

	private:
		std::unique_ptr<T> p_;
};
