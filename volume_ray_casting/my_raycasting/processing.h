#ifndef CALCULATION_H
#define CALCULATION_H

#include <fstream>

#include "K_Means.h"
#include "K_Means_PP_DIY.h"
#include "K_Means_Local.h"
#include "K_Means_PlusPlus.h"

namespace processing
{
	template <class T, int TYPE_SIZE>
	void k_means(const T *data, const unsigned int count, const unsigned int components, const int k, unsigned char *& label_ptr)
	{
		unsigned int histogram[TYPE_SIZE] = {0};
		vector<float> scalar_value(count); // the scalar data in const T *data
		vector<nv::vec3f> gradient(count);
		vector<float> gradient_magnitude(count);
		vector<nv::vec3f> second_derivative(count);
		vector<float> second_derivative_magnitude(count);
		float max_gradient_magnitude, max_second_derivative_magnitude;
		generate_scalar_histogram<T, TYPE_SIZE>(data, count, components, histogram, scalar_value);
		generate_gradient(sizes, count, components, scalar_value, gradient, gradient_magnitude, max_gradient_magnitude, second_derivative, second_derivative_magnitude, max_second_derivative_magnitude);

		// clustering
		K_Means_PP_DIY::k_means(count, scalar_value, gradient_magnitude, second_derivative_magnitude, k, label_ptr);
	}

	// calculate scalar histogram
	template <class T, int TYPE_SIZE>
	void generate_scalar_histogram(const T *data, const unsigned int count, const unsigned int components, unsigned int *histogram, vector<float> &scalar_value)
	{
		unsigned int temp, index;

		for (unsigned int i=0; i<count; i++)
		{
			temp = 0;
			index = i * components;
			for (unsigned int j=0; j<components; j++)
			{
				temp += data[index + j];
			}
			temp = temp / components;
			histogram[temp]++;
			scalar_value[i] = temp;
		}
	}

	// find the min and max scalar value for histogram equalization in shaders
	template <class T, int TYPE_SIZE>
	void find_min_max_scalar_in_histogram(const unsigned int count, const unsigned int *histogram, float &scalar_min, float &scalar_max)
	{
		const unsigned int min_amount = static_cast<unsigned int>(count / 64.0);
		unsigned int amount = min_amount;
		unsigned int min_index = 0;
		for (unsigned int i=0; i<TYPE_SIZE; i++)
		{
			if (histogram[i] >= amount)
			{
				min_index = i;
				break;
			}else
			{
				amount -= histogram[i];
			}
		}
		amount = min_amount;
		unsigned int max_index = TYPE_SIZE - 1;
		for (unsigned int i=TYPE_SIZE-1; i>=0; i--)
		{
			if (histogram[i] >= amount)
			{
				max_index = i;
				break;
			}else
			{
				amount -= histogram[i];
			}
		}
		scalar_min = static_cast<float>(min_index) / TYPE_SIZE;
		scalar_max = static_cast<float>(max_index) / TYPE_SIZE;

#ifdef _DEBUG
		std::cout<<"min max index\t"<<min_index<<"\t"<<max_index<<endl
			<<"min max float\t"<<scalar_min<<"\t"<<scalar_max<<endl;
#endif
	}

	// calculate the gradients and the second derivatives
	void generate_gradient(const int *sizes, const unsigned int count, const unsigned int components, const vector<float> &scalar_value, vector<nv::vec3f> &gradient, vector<float> &gradient_magnitude, float &max_gradient_magnitude, vector<nv::vec3f> &second_derivative, vector<float> &second_derivative_magnitude, float &max_second_derivative_magnitude)
	{
		unsigned int index;
		int boundary[3] = {sizes[0]-1, sizes[1]-1, sizes[2]-1};
		//nv::vec3f second_derivative;

		max_second_derivative_magnitude = max_gradient_magnitude = -1;
		for (int i=0; i<sizes[0]; i++)
		{
			for (int j=0; j<sizes[1]; j++)
			{
				for (int k=0; k<sizes[2]; k++)
				{
					index = i * j * k;
					if (i==0 || j==0 || k==0 || i==boundary[0] || j==boundary[1] || k==boundary[2])
					{
						gradient_magnitude[index] = gradient[index].x = gradient[index].y = gradient[index].z = 0;
					}else
					{
						gradient[index].x = scalar_value[(i+1) * j * k] - scalar_value[(i-1) * j * k];
						gradient[index].y = scalar_value[i * (j+1) * k] - scalar_value[i * (j-1) * k];
						gradient[index].z = scalar_value[i * j * (k+1)] - scalar_value[i * j * (k-1)];
						gradient_magnitude[index] = length(gradient[index]);
						max_gradient_magnitude = max(gradient_magnitude[index], max_gradient_magnitude);
					}
				}
			}
		}

		for (int i=0; i<sizes[0]; i++)
		{
			for (int j=0; j<sizes[1]; j++)
			{
				for (int k=0; k<sizes[2]; k++)
				{
					index = i * j * k;
					if (i==0 || j==0 || k==0 || i==boundary[0] || j==boundary[1] || k==boundary[2])
					{
						second_derivative_magnitude[index] = 0;
					}else
					{
						second_derivative[index].x = gradient[(i+1) * j * k].x - gradient[(i-1) * j * k].x;
						second_derivative[index].y = gradient[i * (j+1) * k].y - gradient[i * (j-1) * k].y;
						second_derivative[index].z = gradient[i * j * (k+1)].z - gradient[i * j * (k-1)].z;
						second_derivative_magnitude[index] = length(second_derivative[index]);
						max_second_derivative_magnitude = max(second_derivative_magnitude[index], max_second_derivative_magnitude);
					}
				}
			}
		}
	}

}

#endif // CALCULATION_H
