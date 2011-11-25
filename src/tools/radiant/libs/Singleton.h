#ifndef SINGLETON_H_
#define SINGLETON_H_

template<class T>
class Singleton {

private:

	Singleton<T> &operator= (const Singleton<T> &);
	Singleton<T> (const Singleton<T> &);

protected:

	static T *_singleton;

public:

	virtual ~Singleton ()
	{
	}

	static T& Instance ()
	{
		// single threaded design!
		if (!_singleton)
			_singleton = new T();
		return *_singleton;
	}
};

#endif /* SINGLETON_H_ */
