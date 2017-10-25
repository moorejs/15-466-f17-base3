// a platform independent uniform and normal distribution

// code below is from https://stackoverflow.com/a/34962942
template <typename T>
class UniformRealDistribution
{
		typedef T result_type;

 public:
		UniformRealDistribution(T _a = 0.0, T _b = 1.0)
				:m_a(_a),
				 m_b(_b)
		{}

		void reset() {}

		template <class Generator>
		T operator()(Generator &_g)
		{
				double dScale = (m_b - m_a) / ((T)(_g.max() - _g.min()) + (T)1); 
				return (_g() - _g.min()) * dScale  + m_a;
		}

		T a() const {return m_a;}
		T b() const {return m_b;}

 protected:
		T       m_a;
		T       m_b;
};

template <typename T>
class NormalDistribution
{
 public:
		typedef T result_type;

 public:
		NormalDistribution(T _mean = 0.0, T _stddev = 1.0)
				:m_mean(_mean),
				 m_stddev(_stddev)
		{}

		void reset()
		{
				m_distU1.reset();
		}

		template <class Generator>
		T operator()(Generator &_g)
		{
				// Use Box-Muller algorithm
				const double pi = 3.14159265358979323846264338327950288419716939937511;
				double u1 = m_distU1(_g);
				double u2 = m_distU1(_g);
				double r = sqrt(-2.0 * log(u1));
				return m_mean + m_stddev * r * sin(2.0 * pi * u2);
		}

		T mean() const {return m_mean;}
		T stddev() const {return m_stddev;}

protected:
		T                           m_mean;
		T                           m_stddev;
		UniformRealDistribution<T>  m_distU1;
};
