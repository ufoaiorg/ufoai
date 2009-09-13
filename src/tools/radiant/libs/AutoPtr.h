#ifndef AUTOPTR_H
#define AUTOPTR_H

template<typename T> class AutoPtr
{
	public:
		explicit AutoPtr (T* const p = 0) :
			p_(p)
		{
		}

		~AutoPtr ()
		{
			if (p_)
				delete p_;
		}

		void deallocate ()
		{
			*this = 0;
		}

		T* release ()
		{
			T* const p = p_;
			p_ = 0;
			return p;
		}

		void operator = (T* const p)
		{
			if (p_)
				delete p_;
			p_ = p;
		}

		T* operator -> () const
		{
			return p_;
		}

		operator T* () const
		{
			return p_;
		}

		operator bool () const
		{
			return p_;
		}

	private:
		T* p_;

		AutoPtr (const AutoPtr&); /* no copy */
		void operator = (AutoPtr&); /* no assignment */
};

#endif
