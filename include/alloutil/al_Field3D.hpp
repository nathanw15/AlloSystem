#ifndef INC_AL_UTIL_FIELD3D_HPP
#define INC_AL_UTIL_FIELD3D_HPP

#include "allocore/types/al_Array.hpp"
#include "allocore/math/al_Functions.hpp"

/*!
	A collection of utilities for 3D fields
*/

namespace al {

/*!
	Field processing often requires double-buffering
*/

template<typename T=double>
class Field3D {
public:
	Field3D(int components, int dim) 
	:	mDim(ceilPow2(dim)),
		mDim3(mDim*mDim*mDim),
		mDimWrap(mDim-1),
		mFront(0) 
	{
		mArrays[0].format(components, Array::type<T>(), mDim, mDim, mDim);
		mArrays[1].format(components, Array::type<T>(), mDim, mDim, mDim);
		mArrays[0].zero();
		mArrays[1].zero();
	}
	
	unsigned length() const { return components()*mDim3; }
	unsigned components() const { return mArrays[0].header.components; }
	unsigned dim() const { return mDim; }
	unsigned stride(int dim=0) const { return mArrays[0].header.stride[dim]; }
	
	// front is what is currently interacted with
	// back is used for intermediate processing
	Array& front() { return mArrays[mFront]; }
	Array& back() { return mArrays[!mFront]; }
	
	// raw access to internal pointer
	T * ptr() { return (T *)front().data.ptr; }
	// pointer index of a particular cell:
	unsigned index(int x, int y, int z) const;
	// access a particular element:
	T& cell(int x, int y, int z, int k=0);
	
	// swap buffers:
	void swap() { mFront = !mFront; }

	// multiply the front array:
	void scale(T v);
	void scale(const Array& src);	// src must have matching layout
	// multiply by 1./(src+1.), as a 'damping' factor:
	void damp(const Array& src);	// src must have matching layout
	// add to front array. src must have matching layout
	void add(Array& src);
	
	// fill with noise:
	void uniform(rnd::Random<>& rng);
	void uniformS(rnd::Random<>& rng);
	// fill with sines:
	void setHarmonic(T px=T(1), T py=T(1), T pz=T(1));
	// 3-component fields only: set velocities to zero at boundaries
	void boundary();
	
	// diffusion
	void diffuse(T diffusion=T(0.01), int passes=10);
	static void diffuse(Array& dst, const Array& src, T diffusion=T(0.01), int passes=10);
	
	// advect a field.
	// velocity field should have 3 components
	void advect(const Array& velocities, T rate = T(1.));
	static void advect(Array& dst, const Array& src, const Array& velocities, T rate = T(1.));
	
	/*
		Clever part of Jos Stam's work.
			A velocity field can become divergent (have regions that are purely emanating or aggregating)
				violating the definition of an incompressible fluid
			But, since a velocity field can be seen as an incompressible velocity field + a gradient field,
				we can subtract a gradient field from our bad velocity field to get an incompressible one
			To calculate this gradient field and then subtract it, we use this function:
	*/
	// grabs the gradient from the field:
	void calculateGradient(Array& gradient, double factor);
	// subtracts this gradient
	void subtractGradient(const Array& gradient);
	
	void relax(double a, int iterations);
	
protected:
	unsigned mDim, mDim3, mDimWrap;
	Array mArrays[2];	// double-buffering
	int mFront;			// which one is the front buffer?
};

template<typename T=double>
class Fluid {
public:
	Fluid(int components, int dim) 
	:	densities(components, dim),
		velocities(3, dim),
		gradient(1, dim),
		passes(10),
		viscocity(0.0001),
		diffusion(0.01),
		decay(0.999)
	{}
	
	~Fluid() {}
	
	/*
		a full fluid simulation:
	*/
	void update() {
		// VELOCITIES:
		// assume new data is in front();
		// smoothen the new data:
		velocities.diffuse(viscocity, passes);
		// (diffused data now in velocities.front())
		// stabilize: 
		project(gradient);
		// (projected data now in velocities.front())
		// advect velocities:
		velocities.advect(velocities.back());
		// (advected data now in velocities.front())
		// stabilize again:
		project(gradient);
		// (projected data now in velocities.front())

		// DENSITIES:
		// assume new data is in front();
		// smoothen the new data:
		densities.diffuse(diffusion, passes);
		//(diffused data now in densities.front())
		// and advect:
		densities.advect(velocities.front());
		//(advected data now in densities.front())
		// fade, etc.
		densities.scale(decay);
	}
	
	void project() {
		// prepare new gradient data:
		gradient.back().zero();
		velocities.calculateGradient(gradient.front());
		// diffuse it:
		gradient.diffuse(1., passes);
		// subtract from current velocities:
		velocities.subtractGradient(gradient.front());
	}
	
	Field3D<T> densities, velocities, gradient;
	unsigned passes;
	T viscocity, diffusion, decay;
};

template<typename T>
unsigned Field3D<T>::index(int x, int y, int z) const {
	return	((x&mDimWrap) * stride(0)) +
			((y&mDimWrap) * stride(1)) +
			((z&mDimWrap) * stride(2));
}

template<typename T>
T& Field3D<T>::cell(int x, int y, int z, int k) {
	return ptr()[index(x, y, z) + k];
}

template<typename T>
void Field3D<T>::setHarmonic(T px, T py, T pz) {
	T vals[3];
	for (int z=0;z<mDim;z++) {
		vals[2] = sin(pz * M_2PI * z/T(dim()));
		for (int y=0;y<mDim;y++) {
			vals[1] = sin(py * M_2PI * y/T(dim()));
			for (int x=0;x<mDim;x++) {
				vals[0] = sin(px * M_2PI * x/T(dim()));
				T value = vals[0] * vals[1] * vals[2];
				front().write(&value, x, y, z);
			}
		}
	}
}

template<typename T>
void Field3D<T>::uniformS(rnd::Random<>& rng) {
	T * p = ptr();
	for (int k=0;k<length();k++) p[k] = rng.uniformS();
}
template<typename T>
void Field3D<T>::uniform(rnd::Random<>& rng) {
	T * p = ptr();
	for (int k=0;k<length();k++) p[k] = rng.uniform();
}

template<typename T>
inline void Field3D<T>::scale(T v) {
	T * p = ptr();
	for (int k=0;k<length();k++) p[k] *= v;
}

template<typename T>
inline void Field3D<T>::scale(const Array& arr) {
	if (arr.isFormat(front().header)) {
		const uint32_t stride0 = stride(0);
		const uint32_t stride1 = stride(1);
		const uint32_t stride2 = stride(2);
		#define INDEX(p, x, y, z) ((T *)((p) + (((x)&mDimWrap)*stride0) +  (((y)&mDimWrap)*stride1) +  (((z)&mDimWrap)*stride2)))

		// zero the boundary fields
		char * optr = front().data.ptr;
		char * vptr = arr.data.ptr;
		for (int z=0;z<mDim;z++) {
			for (int y=0;y<mDim;y++) {
				for (int x=0;x<mDim;x++) {
					// cell to update:
					T * cell = INDEX(optr, x, y, z);
					T v = INDEX(vptr, x, y, z)[0];

					for (int k=0; k<components(); k++) {
						cell[k] *= v;
					}
				}
			}
		}

		#undef INDEX
	} else {
		printf("Array format mismatch\n");
	}
}

template<typename T>
inline void Field3D<T>::damp(const Array& arr) {
	if (arr.isFormat(front().header)) {
		const uint32_t stride0 = stride(0);
		const uint32_t stride1 = stride(1);
		const uint32_t stride2 = stride(2);
		#define INDEX(p, x, y, z) ((T *)((p) + (((x)&mDimWrap)*stride0) +  (((y)&mDimWrap)*stride1) +  (((z)&mDimWrap)*stride2)))

		// zero the boundary fields
		char * optr = front().data.ptr;
		char * vptr = arr.data.ptr;
		for (int z=0;z<mDim;z++) {
			for (int y=0;y<mDim;y++) {
				for (int x=0;x<mDim;x++) {
					// cell to update:
					T * cell = INDEX(optr, x, y, z);
					T v = fabs(INDEX(vptr, x, y, z)[0]);

					for (int k=0; k<components(); k++) {
						cell[k] *= 1./(v+1.);
					}
				}
			}
		}

		#undef INDEX
	} else {
		printf("Array format mismatch\n");
	}
}
	
template<typename T>
inline void Field3D<T>::add(Array& src) {
	if (src.isFormat(front().header)) {
		T * in  = (T *)src.data.ptr;
		T * out = (T *)front().data.ptr;
		for (int i=0; i<mDim3; i++) {
			out[i] += in[i];
			in[i] = T(0);
		}
	} else {
		printf("Array format mismatch\n");
	}
}

template<typename T>
inline void Field3D<T> :: diffuse(T diffusion, int passes) {
	swap(); 
	diffuse(front(), back(), diffusion, passes);
}

template<typename T>
inline void Field3D<T> :: relax( double a, int iterations) {
	char * old = front().data.ptr;
	char * out = back().data.ptr;
	const uint32_t comps = components();
	const double c = 1./(1. + 6.*a); 
	for (int iter=0; iter<iterations; iter++) {
		for (int z=0;z<mDim;z++) {
			for (int y=0;y<mDim;y++) {
				for (int x=0;x<mDim;x++) {
					const unsigned idx = index(x, y, z);
					const unsigned x0 = index(x-1, y, z);
					const unsigned x1 = index(x+1, y, z);
					const unsigned y0 = index(x, y-1, z);
					const unsigned y1 = index(x, y+1, z);
					const unsigned z0 = index(x, y, z-1);
					const unsigned z1 = index(x, y, z+1);
					for (unsigned k=0; k<comps; k++) {
						out[idx+k] = c * (
										old[idx+k] +
										a * (	
											out[x0+k] + out[x1+k] +
											out[y0+k] + out[y1+k] +
											out[z0+k] + out[z1+k]
										)
									);
					}
				}
			}
		}
		// todo: apply boundary here
	}
	#undef CELL
}

template<typename T>
inline void Field3D<T> :: diffuse(Array& out, const Array& in, T diffusion, int passes) {
	const uint32_t stride0 = in.header.stride[0];
	const uint32_t stride1 = in.header.stride[1];
	const uint32_t stride2 = in.header.stride[2];
	const uint32_t components = in.header.components;
	const uint32_t dim = in.header.dim[0];
	char * iptr = in.data.ptr;
	char * optr = out.data.ptr;
	double div = 1.0/((1.+6.*diffusion));

	// relaxation scheme:
	for (int n=0 ; n<passes ; n++) {
		for (int z=0;z<dim;z++) {
			for (int y=0;y<dim;y++) {
				for (int x=0;x<dim;x++) {
					T * next =	optr + index(x,	y,	z);
					T * prev =	iptr + index(x,	y,	z);
					T * va00 =	optr + index(x-1,y,	z);
					T * vb00 =	optr + index(x+1,y,	z);
					T * v0a0 =	optr + index(x,	y-1,z);
					T * v0b0 =	optr + index(x,	y+1,z);
					T * v00a =	optr + index(x,	y,	z-1);
					T * v00b =	optr + index(x,	y,	z+1);
					for (int k=0;k<components;k++) {
						next[k] = div*(
							prev[k] +
							diffusion * (
								va00[k] + vb00[k] +
								v0a0[k] + v0b0[k] +
								v00a[k] + v00b[k]
							)
						);
					}
				}
			}
		}
	}
}

template<typename T>
inline void Field3D<T> :: advect(Array& dst, const Array& src, const Array& velocities, T rate) {
	const uint32_t stride0 = src.stride(0);
	const uint32_t stride1 = src.stride(1);
	const uint32_t stride2 = src.stride(2);
	const uint32_t dim = src.dim(0);
	const uint32_t dimwrap = dim-1;
	
	if (velocities.header.type != src.header.type ||
		velocities.header.components < 3 ||
		velocities.header.dim[0] != dim ||
		velocities.header.dim[1] != dim ||
		velocities.header.dim[2] != dim) 
	{
		printf("Array format mismatch\n");
		return;
	}
	char * outptr = dst.data.ptr;
	char * velptr = velocities.data.ptr;

	#define CELL(p, x, y, z, k) (((T *)((p) + (((x)&dimwrap)*stride0) +  (((y)&dimwrap)*stride1) +  (((z)&dimwrap)*stride2)))[(k)])

	for (int z=0;z<dim;z++) {
		for (int y=0;y<dim;y++) {
			for (int x=0;x<dim;x++) {
				// back trace: (current cell offset by vector at cell)
				T * bp  = &(CELL(outptr, x, y, z, 0));
				T * vp	= &(CELL(velptr, x, y, z, 0));
				T vx = x - rate * vp[0];
				T vy = y - rate * vp[1];
				T vz = z - rate * vp[2];

				// read interpolated input field value into back-traced location:
				src.read_interp(bp, vx, vy, vz);
			}
		}
	}
	#undef CELL
}

template<typename T>
inline void Field3D<T> :: advect(const Array& velocities, T rate) {
	swap();
	advect(front(), back(), velocities, rate);
}

template<typename T>
inline void Field3D<T> :: calculateGradient(Array& gradient, double factor) {
	gradient.format(1, Array::type<T>(), mDim, mDim, mDim);
	const uint32_t stride0 = stride(0);
	const uint32_t stride1 = stride(1);
	const uint32_t stride2 = stride(2);
	
	#define CELL(p, x, y, z, k) (((T *)((p) + (((x)&mDimWrap)*stride0) +  (((y)&mDimWrap)*stride1) +  (((z)&mDimWrap)*stride2)))[(k)])

	// calculate gradient.
	// previous instantaneous magnitude of velocity gradient
	//		= average of velocity gradients per axis:
	const double h = 1./3.; //0.5/mDim;
	char * iptr = front().data.ptr;
	char * optr = gradient.data.ptr;
	for (int z=0;z<mDim;z++) {
		for (int y=0;y<mDim;y++) {
			for (int x=0;x<mDim;x++) {
				// gradients per axis:
				T xgrad =	CELL(iptr, x-1,y,	z,	0)-CELL(iptr, x+1,y,	z,	0);
				T ygrad =	CELL(iptr, x,	y-1,z,	1)-CELL(iptr, x,	y+1,z,	1);
				T zgrad =	CELL(iptr, x,	y,	z-1,2)-CELL(iptr, x,	y,	z+1,2);
				// gradient at current cell:
				T grad = h * (xgrad+ygrad+zgrad);
				// store in a 1-plane field
				CELL(optr, x, y, z, 0) = grad;
			}
		}
	}
	#undef CELL
}



template<typename T>
inline void Field3D<T> :: subtractGradient(const Array& gradient) {
	if (gradient.header.type != Array::type<T>() ||
		gradient.header.dim[0] != mDim ||
		gradient.header.dim[1] != mDim ||
		gradient.header.dim[2] != mDim) 
	{
		printf("Array format mismatch\n");
		return;
	}
	const uint32_t stride0 = stride(0);
	const uint32_t stride1 = stride(1);
	const uint32_t stride2 = stride(2);
	#define CELL(p, x, y, z, k) (((T *)((p) + (((x)&mDimWrap)*stride0) +  (((y)&mDimWrap)*stride1) +  (((z)&mDimWrap)*stride2)))[(k)])
	
	// now subtract gradient from current field:
	char * gptr = gradient.data.ptr;
	char * optr = front().data.ptr;
	const double h = -0.5*mDim;
	for (int z=0;z<mDim;z++) {
		for (int y=0;y<mDim;y++) {
			for (int x=0;x<mDim;x++) {
				// cell to update:
				T * vel = &CELL(optr, x, y, z, 0);
				// gradients per axis:
				vel[0] -= h * ( CELL(gptr, x-1,y,	z,	0)-CELL(gptr, x+1,y,	z,	0) );
				vel[1] -= h * ( CELL(gptr, x,	y-1,z,	0)-CELL(gptr, x,	y+1,z,	0) );
				vel[2] -= h * ( CELL(gptr, x,	y,	z-1,0)-CELL(gptr, x,	y,	z+1,0) );
			}
		}
	}

	#undef CELL
}

template<typename T>
inline void Field3D<T> :: boundary() {
	if (front().header.components < 3) {
		printf("only valid for 3-component fields\n");
		return;
	}
	
	// x planes
	for (int z=0;z<mDim;z++) {
		for (int y=0;y<mDim;y++) {
			cell(0, y, z, 0) = T(0);
			cell(mDim, y, z, 0) = T(0); 
		}
	}
	// y planes
	for (int z=0;z<mDim;z++) {
		for (int x=0;x<mDim;x++) {
			cell(x, 0, z, 1) = T(0);
			cell(x, mDim, z, 1) = T(0); 
		}
	}
	// z planes
	for (int y=0;y<mDim;y++) {
		for (int x=0;x<mDim;x++) {
			cell(x, y, 0, 2) = T(0);
			cell(x, y, mDim, 2) = T(0); 
		}
	}
}


}; // al
#endif