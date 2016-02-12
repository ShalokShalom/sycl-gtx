#pragma once

#include <sycl.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <memory>


#ifndef float_type
#define float_type double
#endif

#include "classes.h"
#include "win.h"

#ifndef modify_sample_rate
#define modify_sample_rate 1
#endif

using Vec = Vec_<float_type>;
using Ray = Ray_<float_type>;
using Sphere = Sphere_<float_type, modify_sample_rate>;


using std::string;

inline float_type clamp(float_type x) {
	return x < 0 ? 0 : x>1 ? 1 : x;
}
inline int toInt(float_type x);

static void to_file(int w, int h, Vec* c, string filename) {
	FILE* f = fopen(filename.c_str(), "w");         // Write image to PPM file.
	fprintf(f, "P3\n%d %d\n%d\n", w, h, 255);
	for(int i = 0; i < w*h; i++) {
		fprintf(f, "%d %d %d\n", toInt(c[i].x), toInt(c[i].y), toInt(c[i].z));
	}
	fclose(f);
}

using time_point = std::chrono::high_resolution_clock::time_point;

static auto now = []() {
	return std::chrono::high_resolution_clock::now();
};
static auto duration = [](time_point before) {
	static const float to_seconds = 1e-6f;
	return std::chrono::duration_cast<std::chrono::microseconds>(now() - before).count() * to_seconds;
};

struct testInfo {
	using function_ptr = void(*)(void*, int, int, int, Ray, Vec, Vec, Vec, Vec*);
	string name;
	function_ptr test;
	std::shared_ptr<cl::sycl::device> dev;
	float lastTime = 0;

	testInfo(string name, function_ptr test, std::shared_ptr<cl::sycl::device> dev = nullptr)
		: name(name), test(test), dev(dev) {}

	testInfo(const testInfo&) = delete;
	testInfo(testInfo&& move)
		: name(std::move(move.name)), test(move.test), dev(std::move(move.dev)) {}

	bool isOpenCL() {
		return dev.get() != nullptr;
	}
};

static decltype(now())& startTime() {
	static decltype(now()) s(now());
	return s;
}

static Ray& cam() {
	static Ray c(Vec(50, 52, 295.6), Vec(0, -0.042612, -1).norm()); // cam pos, dir
	return c;
}

static string& imagePrefix() {
	static string ip;
	return ip;
}

static bool& isOpenclAvailable() {
	static bool ocl(false);
	return ocl;
}

static bool tester(std::vector<testInfo>& tests, int w, int h, int samples, Vec& cx, Vec& cy, int iterations, int from, int to) {
	using namespace std;

	if(tests.empty()) {
		cout << "no tests" << endl;
		return false;
	}

	cout << "samples per pixel: " << samples << endl;

	Vec r;
	vector<Vec> empty_vectors(w*h, 0);
	vector<Vec> vectors;
	float time;

	const float perTestLimit = 40;
	const float globalLimit = 420;
	float totalTime = 0;

	for(int ti = from; ti < to; ++ti) {
		auto& t = tests[ti];

		// Quality of Service
		// Prevents the test from taking too long, but also allows it to use up as much time as possible
		// OpenCL tests are preferred
		bool overHalf = 2 * totalTime > globalLimit;
		if(t.lastTime > perTestLimit &&
			(
				(!isOpenclAvailable() && overHalf)	||
				(isOpenclAvailable() &&
					!t.isOpenCL() ||
					(t.isOpenCL() && overHalf)
				)
			)
		) {
			continue;
		}

		cout << "Running test: " << t.name << endl;
		ns_erand::reset();
#ifndef _DEBUG
		try {
#endif
			auto start = now();
			for(int i = 0; i < iterations; ++i) {
				vectors = empty_vectors;
				t.test(t.dev.get(), w, h, samples, cam(), cx, cy, r, vectors.data());
			}
			time = (duration(start) / (float)iterations);

#ifndef _DEBUG
		}
		catch(cl::sycl::exception& e) {
			cerr << "SYCL error while testing: " << e.what() << endl;
			continue;
		}
		catch(std::exception& e) {
			cerr << "error while testing: " << e.what() << endl;
			continue;
		}
#endif

#ifdef _DEBUG
		to_file(w, h, vectors.data(), imagePrefix() + ' ' + t.name + ".ppm");
#endif

		cout << "time: " << time << endl;
		t.lastTime = time;
		totalTime = duration(startTime());
		if(totalTime > globalLimit) {
			cout << "exceeded " + std::to_string((int)globalLimit) + "s limit, stopping" << endl;
			return false;
		}
	}
	
	return true;
}

struct version {
	int major = 0;
	int minor = 0;

	version(int major, int minor)
		: major(major), minor(minor) {}
	version(const string& v) {
		// https://www.khronos.org/registry/cl/sdk/1.2/docs/man/xhtml/clGetPlatformInfo.html
		using namespace std;
		string search("OpenCL");
		auto pos = v.find(search);
		if(pos != string::npos) {
			pos += search.length() + 1;	// Plus one for space
			try {
				major = (int)v.at(pos) - '0';
				minor = (int)v.at(pos + 2) - '0';; // Plus one for dot
			}
			catch(std::exception&) {}
		}
	}
};

template <class T>
void printInfo(string description, const T& data, int offset = 0) {
	string indent;
	for(int i = 0; i < offset; ++i) {
		indent += '\t';
	}
	std::cout << indent << description << ": " << data << std::endl;
}

static void getDevices(std::vector<testInfo>& tests, std::vector<testInfo::function_ptr> compute_sycl_ptrs) {
	using namespace std;

	try {
		using namespace cl::sycl;

		auto platforms = platform::get_platforms();
		std::vector<testInfo> tests_;

		version required(1, 2);

		int pNum = 0;
		for(auto& p : platforms) {
			cout << "- OpenCL platform " << pNum << ':' << endl;
			++pNum;

			auto openclVersion = p.get_info<info::platform::version>();

			version platformVersion(openclVersion);

			printInfo("name", p.get_info<info::platform::name>(), 1);
			printInfo("vendor", p.get_info<info::platform::vendor>(), 1);
			printInfo("version", openclVersion, 1);
			printInfo("profile", p.get_info<info::platform::profile>(), 1);
			printInfo("extensions", p.get_info<info::platform::extensions>(), 1);

			auto devices = p.get_devices();
			int dNum = 0;

			for(auto& d : devices) {
				cout << "\t-- OpenCL device " << dNum << ':' << endl;

				auto name = d.get_info<info::device::name>();

				printInfo("name", name, 2);
				printInfo("device_type", (cl_device_type)d.get_info<info::device::device_type>(), 2);
				printInfo("vendor", d.get_info<info::device::vendor>(), 2);
				printInfo("device_version", d.get_info<info::device::device_version>(), 2);
				printInfo("driver_version", d.get_info<info::device::driver_version>(), 2);
#ifdef SYCL_GTX
				printInfo("opencl_version", d.get_info<info::device::opencl_version>(), 2);
				printInfo("single_fp_config", d.get_info<info::device::single_fp_config>(), 2);
				printInfo("double_fp_config", d.get_info<info::device::double_fp_config>(), 2);
#endif
				printInfo("profile", d.get_info<info::device::profile>(), 2);
				printInfo("error_correction_support", d.get_info<info::device::error_correction_support>(), 2);
				printInfo("host_unified_memory", d.get_info<info::device::host_unified_memory>(), 2);
				printInfo("max_clock_frequency", d.get_info<info::device::max_clock_frequency>(), 2);
				printInfo("max_compute_units", d.get_info<info::device::max_compute_units>(), 2);
				printInfo("max_work_item_dimensions", d.get_info<info::device::max_work_item_dimensions>(), 2);
				printInfo("max_work_group_size", d.get_info<info::device::max_work_group_size>(), 2);

				printInfo("address_bits", d.get_info<info::device::address_bits>(), 2);
				printInfo("max_mem_alloc_size", d.get_info<info::device::max_mem_alloc_size>(), 2);
				printInfo("global_mem_cache_line_size", d.get_info<info::device::global_mem_cache_line_size>(), 2);
				printInfo("global_mem_cache_size", d.get_info<info::device::global_mem_cache_size>(), 2);
				printInfo("global_mem_size", d.get_info<info::device::global_mem_size>(), 2);
				printInfo("max_constant_buffer_size", d.get_info<info::device::max_constant_buffer_size>(), 2);
				printInfo("max_constant_args", d.get_info<info::device::max_constant_args>(), 2);
				printInfo("local_mem_size", d.get_info<info::device::local_mem_size>(), 2);
				printInfo("extensions", d.get_info<info::device::extensions>(), 2);

				if(
					platformVersion.major > required.major ||
					(platformVersion.major == required.major && platformVersion.minor >= required.minor)
				) {
#ifndef SYCL_GTX
#ifdef _DEBUG
					// There seem to be some problems with ComputeCpp and HD 4600
					if(name.find("HD Graphics 4600") == string::npos)
#endif
#endif
					tests_.emplace_back(name + ' ' + openclVersion, nullptr, std::shared_ptr<device>(new device(std::move(d))));
				}

				++dNum;
			}
		}

		int i = 0;
		for(auto ptr : compute_sycl_ptrs) {
			++i;
			for(auto& t : tests_) {
				tests.emplace_back(string("T") + std::to_string(i) + ' ' + t.name, ptr, t.dev);
			}
		}

		isOpenclAvailable() = true;
	}
	catch(cl::sycl::exception& e) {
		// TODO
		cout << "OpenCL not available: " << e.what() << endl;
		
		isOpenclAvailable() = false;
	}
}

static int mainTester(int argc, char *argv[], std::vector<testInfo>& tests, string image_prefix) {
	using namespace std;

	cout << "smallpt SYCL tester" << endl;

	imagePrefix() = image_prefix;

	int w = 1024;
	int h = 768;
	Vec cx = Vec(w*.5135 / h);
	Vec cy = (cx%cam().d).norm()*.5135;
	auto numTests = tests.size();

	int from = 0;
	int to = numTests;
	if(argc > 1) {
		from = atoi(argv[1]);
		if(argc > 2) {
			to = atoi(argv[2]);
		}
	}

	cout << "Going through tests in range [" << from << ',' << to << ')' << endl;

	if(false) {
		tester(tests, w, h, 1, cx, cy, 1, from, to);
		cout << "Press any key to exit" << endl;
		cin.get();
		return 0;
	}

	// Test suite
	int iterations = 1;
	bool canContinue;

	for(int samples = 4; samples < 10000; samples *= 2) {
		canContinue = tester(tests, w, h, samples, cx, cy, iterations, from, to);
		if(!canContinue) {
			break;
		}
	}

	auto time = duration(startTime());
	cout << "total test suite duration: " << time << endl;

	//cout << "Press any key to exit" << endl;
	//cin.get();

	return 0;
}