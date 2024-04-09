//生产者消费者,线程调度
#include <thread>
#include <functional>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <mutex>
#include <string>
#include <condition_variable>

class dispatch_queue
{
	typedef std::function<void(void)> fp_t;

  public:
	dispatch_queue(std::string name, size_t thread_cnt = 1);
	~dispatch_queue();

	// dispatch and copy
	void dispatch(const fp_t& op);
	// dispatch and move
	void dispatch(fp_t&& op);

	// Deleted operations
	dispatch_queue(const dispatch_queue& rhs) = delete;
	dispatch_queue& operator=(const dispatch_queue& rhs) = delete;
	dispatch_queue(dispatch_queue&& rhs) = delete;
	dispatch_queue& operator=(dispatch_queue&& rhs) = delete;

  private:
	std::string name_;
	std::mutex lock_;
	std::vector<std::thread> threads_;
	std::queue<fp_t> q_;
	std::condition_variable cv_;
	bool quit_ = false;

	void dispatch_thread_handler(void);
};

dispatch_queue::dispatch_queue(std::string name, size_t thread_cnt) :
	name_{std::move(name)}, threads_(thread_cnt)
{
	printf("Creating dispatch queue: %s\n", name_.c_str());
	printf("Dispatch threads: %zu\n", thread_cnt);

	for(size_t i = 0; i < threads_.size(); i++)
	{
		threads_[i] = std::thread(&dispatch_queue::dispatch_thread_handler, this);
	}
}

dispatch_queue::~dispatch_queue()
{
	printf("Destructor: Destroying dispatch threads...\n");

	// Signal to dispatch threads that it's time to wrap up
	std::unique_lock<std::mutex> lock(lock_);
	quit_ = true;
	cv_.notify_all();
	lock.unlock();

	// Wait for threads to finish before we exit
	for(size_t i = 0; i < threads_.size(); i++)
	{
		if(threads_[i].joinable())
		{
			printf("Destructor: Joining thread %zu until completion\n", i);
			threads_[i].join();
		}
	}
}

void dispatch_queue::dispatch(const fp_t& op)
{
	std::unique_lock<std::mutex> lock(lock_);
	q_.push(op);
	cv_.notify_one();
}

void dispatch_queue::dispatch(fp_t&& op)
{
	std::unique_lock<std::mutex> lock(lock_);
	q_.push(std::move(op));
	cv_.notify_one();
}

void dispatch_queue::dispatch_thread_handler(void)
{
	std::unique_lock<std::mutex> lock(lock_);

	do
	{
		// Wait until we have data or a quit signal
		cv_.wait(lock, [this] {
			return (q_.size() || quit_);
		});

		// after wait, we own the lock
		if(!quit_ && q_.size())
		{
			auto op = std::move(q_.front());
			q_.pop();

			// unlock now that we're done messing with the queue
			lock.unlock();

			op();

			lock.lock();
		}
	} while(!quit_);
}

int main(void)
{
	int r = 0;
	dispatch_queue q("Phillip's Demo Dispatch Queue", 4);

	q.dispatch([] {
		printf("Dispatch 1!\n");
	});
	q.dispatch([] {
		printf("Dispatch 2!\n");
	});
	q.dispatch([] {
		printf("Dispatch 3!\n");
	});
	q.dispatch([] {
		printf("Dispatch 4!\n");
	});

	return r;
}




/** 
 回调例子
 * Example code for:
 *   - callback storage with std::vector
 *   - use of std::function
 *   - use of std::bind
 *   - use of lambda function as callback
 */

#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

/**
 * Basic std::function callback example. Templated on a function signature
 * that takes in a uint32_t and returns void.
 */
typedef std::function<void(uint32_t)> cb_t;

/**
 * Here's an example type that stores a std::function and an argument
 * to pass with that callback.  Passing a uint32_t input to your callback
 * is a pretty common implementation. Also useful if you need to store a
 * pointer for use with a BOUNCE function.
 */
struct cb_arg_t
{
	// the callback - takes a uint32_t input.
	std::function<void(uint32_t)> cb;
	// value to return with the callback.
	uint32_t arg;
};

/**
 * Alternative storage implementation.  Perhaps you want to store
 * callbacks for different event types?
 */
enum my_events_t
{
	VIDEO_STOP = 0,
	VIDEO_START,
	EVENT_MAX
};

struct cb_event_t
{
	std::function<void(uint32_t)> cb;
	my_events_t event;
};

/**
 * Basic example.  Constructed with a uint32_t.
 * Callbacks are passed this uint32_t.
 */
class BasicDriver
{
  public:
	BasicDriver(uint32_t val) : val_(val), callbacks_()
	{
	}

	// Register a callback.
	void register_callback(const cb_t& cb)
	{
		// add callback to end of callback list
		callbacks_.push_back(cb);
	}

	/// Call all the registered callbacks.
	void callback() const
	{
		// iterate through callback list and call each one
		for(const auto& cb: callbacks_)
		{
			cb(val_);
		}
	}

  private:
	/// Integer to pass to callbacks.
	uint32_t val_;
	/// List of callback functions.
	std::vector<cb_t> callbacks_;
};

/**
 * Event based example.  Constructed with a uint32_t.
 * Callbacks are passed this uint32_t.
 * Callbacks are only invoked when their event type matches the
 * occuring event.
 */
class EventDriver
{
  public:
	EventDriver(uint32_t val) : val_(val), callbacks_()
	{
	}

	// Register a callback.
	void register_callback(const cb_t& cb, const my_events_t event)
	{
		// add callback to end of callback list
		callbacks_.push_back({cb, event});
	}

	/// Call all the registered callbacks.
	void callback() const
	{
		my_events_t event = VIDEO_START;
		// iterate through callback list and call each one
		for(const auto& cb: callbacks_)
		{
			if(cb.event == event)
			{
				cb.cb(val_);
			}
		}
	}

  private:
	/// Integer to pass to callbacks.
	uint32_t val_;
	/// List of callback functions.
	std::vector<cb_event_t> callbacks_;
};

/**
 * Arg based example.
 * Callbacks register with a uint32_t that they want returned
 * Callbacks always passed their specific uint32_t value
 */
class ArgDriver
{
  public:
	ArgDriver() : callbacks_()
	{
	}

	// Register a callback.
	void register_callback(const cb_t& cb, const uint32_t val)
	{
		// add callback to end of callback list
		callbacks_.push_back({cb, val});
	}

	/// Call all the registered callbacks.
	void callback() const
	{
		// iterate through callback list and call each one
		for(const auto& cb: callbacks_)
		{
			cb.cb(cb.arg);
		}
	}

  private:
	/// List of callback functions.
	std::vector<cb_arg_t> callbacks_;
};

/**
 * Dummy Client #1
 * Uses a static method for a callback.
 */
class Client1
{
  public:
	static void func(uint32_t v)
	{
		printf("static member callback: 0x%x\n", v);
	}
};

/**
 * Dummy Client #2
 * Uses an instance method as a callback
 */
class Client2
{
  public:
	void func(uint32_t v) const
	{
		printf("instance member callback: 0x%x\n", v);
	}
};

/**
 * Callback on a c function
 */
extern "C"
{
	static void c_client_callback(uint32_t v)
	{
		printf("C function callback: 0x%x\n", v);
	}
}

int main()
{
	/**
	 * Examples using the basic driver
	 * This is constructed with a specific value
	 * Each callback receives this value
	 */
	BasicDriver bd(0xDEADBEEF); // create basic driver instance
	Client2 c2;

	printf("Starting examples using the BasicDriver\n");
	// register a lambda function as a callback
	bd.register_callback([](uint32_t v) {
		printf("lambda callback: 0x%x\n", v);
	});

	/**
	 * std::bind is used to create a function object using
	 * both the instance and method pointers.  This gives you the
	 * callback into the specific object - very useful!
	 *
	 * std::bind keeps specific arguments at a fixed value (or a placeholder)
	 * For our argument here, we keep as a placeholder.
	 */
	bd.register_callback(std::bind(&Client2::func, &c2, std::placeholders::_1));

	// register a static class method as a callback
	bd.register_callback(&Client1::func);

	// register a C function pointer as a callback
	bd.register_callback(&c_client_callback);

	// call all the registered callbacks
	bd.callback();

	printf("End of examples using the BasicDriver\n");

	/**
	 * Examples using the Event Driver.
	 * Note that some callbacks will not be called -
	 * their event type is different!
	 */
	EventDriver ed(0xFEEDBEEF);

	printf("Beginning of examples using the EventDriver\n");

	// register a lambda function as a callback
	ed.register_callback(
		[](uint32_t v) {
			printf("lambda callback: 0x%x\n", v);
		},
		VIDEO_START);

	// register client2 cb using std::bind
	ed.register_callback(std::bind(&Client2::func, &c2, std::placeholders::_1), VIDEO_STOP);

	// register a static class method as a callback
	ed.register_callback(&Client1::func, VIDEO_STOP);

	// register a C function pointer as a callback
	ed.register_callback(&c_client_callback, VIDEO_START);

	// call all the registered callbacks
	ed.callback();

	printf("End of examples using the EventDriver\n");

	/**
	 * Examples using the Arg Driver.
	 * Note that each callback registers for its own value
	 */
	ArgDriver ad;

	printf("Beginning of examples using the ArgDriver\n");

	// register a lambda function as a callback
	ad.register_callback(
		[](uint32_t v) {
			printf("lambda callback: 0x%x\n", v);
		},
		0x0);

	// register client2 cb using std::bind
	ad.register_callback(std::bind(&Client2::func, &c2, std::placeholders::_1), 0x1);

	// register a static class method as a callback
	ad.register_callback(&Client1::func, 0x2);

	// register a C function pointer as a callback
	ad.register_callback(&c_client_callback, 0x3);

	// call all the registered callbacks
	ad.callback();

	printf("End of examples using the ArgDriver\n");

	return 0;
}






















/*************************************************************************
    > File Name: simplest_mediadata_main.cpp
    > Author: zhongjihao
    > Mail: zhongjihao100@163.com 
    > Created Time: Tue 14 Nov 2017 02:57:31 PM CST
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

/////////////////////////////RGB、YUV像素数据处理///////////////////////////////////////

//顺时针旋转90
static void YUV420PClockRot90(unsigned char* dest,unsigned char* src,int w,int h,FILE* fp)
{
	int nPos = 0;
    //旋转Y
	int k = 0;
	for(int i=0;i<w;i++)
	{
		for(int j = h -1;j >=0;j--)
		{
			dest[k++] = src[j*w + i];
		}
	}
	//旋转U
	nPos = w*h;
	for(int i=0;i<w/2;i++)
	{
		for(int j= h/2-1;j>=0;j--)
		{
			dest[k++] = src[nPos+ j*w/2 +i];
		}
	}
   
	//旋转V
	nPos = w*h*5/4;
	for(int i=0;i<w/2;i++)
	{
		for(int j= h/2-1;j>=0;j--)
		{
			dest[k++] = src[nPos+ j*w/2 +i];
		}
	}
	
	fwrite(dest,1,w*h,fp);
	fwrite(dest+w*h,1,w*h/4,fp);
	fwrite(dest+w*h*5/4,1,w*h/4,fp);
}

//顺时针旋转180
static void YUV420PClockRot180(unsigned char* dest,unsigned char* src,int w,int h,FILE* fp)
{ 
	int k = w*h-1;
	//旋转Y
    for(int i=0;i<w*h;i++)
	{
		*(dest + k--) = *(src + i);
	}
	
	//旋转U
	k = w*h*5/4 - 1;
	for(int i = w*h; i < w*h*5/4; i++)
	{	
		*(dest + k--) = *(src + i);
	}
	
	//旋转V
	k = w * h *3/2 -1;
	for(int i = w*h*5/4; i < w*h*3/2; i++)
	{
		*(dest + k--) = *(src + i);
	}
	
	fwrite(dest,1,w*h,fp);
	fwrite(dest+w*h,1,w*h/4,fp);
	fwrite(dest+w*h*5/4,1,w*h/4,fp);
}


/** 
 * Split Y, U, V planes in YUV420P file. 
 * @param url  Location of Input YUV file. 
 * @param w    Width of Input YUV file. 
 * @param h    Height of Input YUV file. 
 * @param num  Number of frames to process. 
 * 
 */
static int simplest_yuv420_split(const char *url, int w, int h,int num)
{
	FILE *fp  = fopen(url,"r+");  
	FILE *fp1 = fopen("out/yuv420p/output_420_y.y","w+");
	FILE *fp2 = fopen("out/yuv420p/output_420_u.y","w+");
    FILE *fp3 = fopen("out/yuv420p/output_420_v.y","w+");

	char yuv[40];
	sprintf(yuv,"out/yuv420p/output_%dx%d_yuv420p.yuv",w,h);
	FILE* fp4 = fopen(yuv,"w+");

	char yuvClockRot90[50];
	sprintf(yuvClockRot90,"out/yuv420p/output_clockrot90_%dx%d_yuv420p.yuv",h,w);
	FILE* fpClockRot90 = fopen(yuvClockRot90,"w+");

	char yuvClockRot180[50];
	sprintf(yuvClockRot180,"out/yuv420p/output_clockrot180_%dx%d_yuv420p.yuv",h,w);
	FILE* fpClockRot180 = fopen(yuvClockRot180,"w+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    unsigned char* picClockRot90 = (unsigned char*)malloc(w*h*3/2);
    unsigned char* picClockRot180 = (unsigned char*)malloc(w*h*3/2);
    
	for(int i=0;i<num;i++)
	{
		 fread(pic,1,w*h*3/2,fp);

		 //Y
		 fwrite(pic,1,w*h,fp1);
		 //U
		 fwrite(pic+w*h,1,w*h/4,fp2);
		 //V
		 fwrite(pic+w*h*5/4,1,w*h/4,fp3);

		 //重新合成yuv420p
         fwrite(pic,1,w*h,fp4);
		 fwrite(pic+w*h,1,w*h/4,fp4);
		 fwrite(pic+w*h*5/4,1,w*h/4,fp4);
       
         //旋转90
		 YUV420PClockRot90(picClockRot90,pic,w,h,fpClockRot90);
         //旋转180
		 YUV420PClockRot180(picClockRot180,pic,w,h,fpClockRot180);
	} 
	 
	free(pic);
	free(picClockRot90);
	free(picClockRot180);
	fclose(fp);  
	fclose(fp1);  
	fclose(fp2);  
	fclose(fp3);
	fclose(fp4);
	fclose(fpClockRot90);
	fclose(fpClockRot180);
}


/** 
* Split Y, U, V planes in YUV444P file. 
* @param url  Location of YUV file. 
* @param w    Width of Input YUV file. 
* @param h    Height of Input YUV file. 
* @param num  Number of frames to process.
*/
static int simplest_yuv444_split(const char *url, int w, int h,int num)
{
	FILE *fp = fopen(url,"r+");  
	FILE *fp1 = fopen("out/yuv444p/output_444_y.y","w+");
	FILE *fp2 = fopen("out/yuv444p/output_444_u.y","w+");
	FILE *fp3 = fopen("out/yuv444p/output_444_v.y","w+");


	char yuv444p[40];
	sprintf(yuv444p,"out/yuv444p/output_%dx%d_yuv444p.yuv",w,h);
	FILE* fp4 = fopen(yuv444p,"w+");

	unsigned char *pic = (unsigned char *)malloc(w*h*3);
    
     for(int i=0;i<num;i++)
	 {
		 fread(pic,1,w*h*3,fp);  
		 //Y  
		 fwrite(pic,1,w*h,fp1);  
		 //U  
		 fwrite(pic+w*h,1,w*h,fp2);  
		 //V 
		 fwrite(pic+w*h*2,1,w*h,fp3);

		 //重新合成yuv444p
         fwrite(pic,1,w*h,fp4);
		 fwrite(pic+w*h,1,w*h,fp4);
		 fwrite(pic+w*h*2,1,w*h,fp4);
	 }
	 
	 free(pic);  
	 fclose(fp);  
	 fclose(fp1);  
	 fclose(fp2);  
	 fclose(fp3);  
					     
	return 0;
}

/** 
* Convert YUV420P file to gray picture 
* @param url     Location of Input YUV file. 
* @param w       Width of Input YUV file. 
* @param h       Height of Input YUV file. 
* @param num     Number of frames to process. 
*/  
static int simplest_yuv420_gray(const char *url, int w, int h,int num)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/yuv420p/output_gray.yuv","w+");
	
	unsigned char *pic = (unsigned char *)malloc(w*h*3/2);
	
	for(int i=0;i<num;i++)
	{
		fread(pic,1,w*h*3/2,fp);
		//Gray
		memset(pic+w*h,128,w*h/2);
		fwrite(pic,1,w*h*3/2,fp1);
	}
	
	free(pic); 
	fclose(fp); 
	fclose(fp1);
	return 0;
}

/** 
* Halve Y value of YUV420P file 
* @param url     Location of Input YUV file. 
* @param w       Width of Input YUV file. 
* @param h       Height of Input YUV file. 
* @param num     Number of frames to process. 
*/  
static int simplest_yuv420_halfy(const char *url, int w, int h,int num)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/yuv420p/output_half.yuv","w+");
		    
	unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for(int i=0;i<num;i++)
	{
		fread(pic,1,w*h*3/2,fp);
        //Half
		for(int j=0;j<w*h;j++)
		{
			unsigned char temp = pic[j]/2;
			pic[j] = temp;
		}
		fwrite(pic,1,w*h*3/2,fp1);
	}

	free(pic); 
	fclose(fp); 
	fclose(fp1);
	
	return 0;  
}

/** 
* Add border for YUV420P file 
* @param url     Location of Input YUV file. 
* @param w       Width of Input YUV file. 
* @param h       Height of Input YUV file. 
* @param border  Width of Border. 
* @param num     Number of frames to process. 
*/
static int simplest_yuv420_border(const char *url, int w, int h,int border,int num)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/yuv420p/output_border.yuv","w+");
	
	unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for(int i=0;i<num;i++)
	{
		fread(pic,1,w*h*3/2,fp);
		//Y
		for(int j=0;j<h;j++)
		{
			for(int k=0;k<w;k++)
			{
				if(k<border|| k > (w-border) || j<border || j>(h-border))
				{
					pic[j*w+k] = 255;
				}
			}
		}
		fwrite(pic,1,w*h*3/2,fp1);
	}
	
	free(pic); 
	fclose(fp); 
	fclose(fp1);
	
	return 0;  
}

/** 
* Generate YUV420P gray scale bar. 
* @param width    Width of Output YUV file. 
* @param height   Height of Output YUV file. 
* @param ymin     Max value of Y 
* @param ymax     Min value of Y 
* @param barnum   Number of bars 
* @param url_out  Location of Output YUV file. 
*/
static int simplest_yuv420_graybar(int width, int height,int ymin,int ymax,int barnum,const char *url_out)
{
	int barwidth;
	float lum_inc;
	unsigned char lum_temp;
	int uv_width,uv_height;  
	FILE *fp = NULL;
	unsigned char *data_y = NULL;
    unsigned char *data_u = NULL;  
	unsigned char *data_v = NULL;
	int t = 0,i = 0,j = 0;
	
	barwidth = width/barnum;
	lum_inc = ((float)(ymax-ymin))/((float)(barnum-1));
	uv_width = width/2;
	uv_height = height/2;
				    
	data_y = (unsigned char *)malloc(width*height);
	data_u = (unsigned char *)malloc(uv_width*uv_height);  
	data_v = (unsigned char *)malloc(uv_width*uv_height);

	if((fp = fopen(url_out,"w+")) == NULL)
	{
		printf("Error: Cannot create file!");
		return -1;
	}
	
	//Output Info
	printf("Y, U, V value from picture's left to right:\n");

	for(t=0;t<(width/barwidth);t++)
	{
		lum_temp = ymin + (char)(t*lum_inc);  
		printf("%3d, 128, 128\n",lum_temp);  
	}

	//Gen Data
	for(j=0;j<height;j++)
	{
		for(i=0;i<width;i++)
		{
			t = i/barwidth;
			lum_temp = ymin + (char)(t*lum_inc);
			data_y[j*width+i] = lum_temp;
		}
	}
	
	for(j=0;j<uv_height;j++)
	{
		for(i=0;i<uv_width;i++)
		{
			data_u[j*uv_width+i] = 128;
		}
	}
	
	for(j=0;j<uv_height;j++)
	{
		for(i=0;i<uv_width;i++)
		{
			data_v[j*uv_width+i] = 128;
		}
	}
	
	fwrite(data_y,width*height,1,fp);  
	fwrite(data_u,uv_width*uv_height,1,fp);  
	fwrite(data_v,uv_width*uv_height,1,fp);  
	
	fclose(fp);
	free(data_y);  
	free(data_u);  
	free(data_v);  
	return 0;
}

/** 
* Calculate PSNR between 2 YUV420P file 
* @param url1     Location of first Input YUV file. 
* @param url2     Location of another Input YUV file. 
* @param w        Width of Input YUV file. 
* @param h        Height of Input YUV file. 
* @param num      Number of frames to process. 
*/
static int simplest_yuv420_psnr(const char *url1,const char *url2,int w,int h,int num)
{
	FILE *fp1 = fopen(url1,"r+");  
	FILE *fp2 = fopen(url2,"r+");
	unsigned char *pic1 = (unsigned char *)malloc(w*h);
	unsigned char *pic2 = (unsigned char *)malloc(w*h);
	
	for(int i=0;i<num;i++)
	{
		fread(pic1,1,w*h,fp1);  
		fread(pic2,1,w*h,fp2);
		
		double mse_sum=0,mse=0,psnr=0;
		for(int j=0;j<w*h;j++)
		{
			mse_sum += pow((double)(pic1[j]-pic2[j]),2);
		}
		mse = mse_sum/(w*h); 
		psnr = 10*log10(255.0*255.0/mse); 
		printf("%5.3f\n",psnr);
		
		fseek(fp1,w*h/2,SEEK_CUR);
		fseek(fp2,w*h/2,SEEK_CUR);
	}

	free(pic1);
	free(pic2); 
	fclose(fp1);  
	fclose(fp2);
	return 0;
}

/** 
* Split R, G, B planes in RGB24 file. 
* @param url  Location of Input RGB file. 
* @param w    Width of Input RGB file. 
* @param h    Height of Input RGB file. 
* @param num  Number of frames to process. 
*/
static int simplest_rgb24_split(const char *url, int w, int h,int num)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/rgb24/output_r.y","w+");
	FILE *fp2 = fopen("out/rgb24/output_g.y","w+");
	FILE *fp3 = fopen("out/rgb24/output_b.y","w+");
				    
	unsigned char *pic = (unsigned char *)malloc(w*h*3);
	
	for(int i=0;i<num;i++)
	{
		fread(pic,1,w*h*3,fp);
		
		for(int j=0;j<w*h*3;j=j+3)
		{
			//R
			fwrite(pic+j,1,1,fp1);
			//G
			fwrite(pic+j+1,1,1,fp2);
			//B
			fwrite(pic+j+2,1,1,fp3);
		}
	}
    
	free(pic); 
	fclose(fp); 
	fclose(fp1);  
	fclose(fp2);
	fclose(fp3);
	
	return 0;
}

/** 
* Convert RGB24 file to BMP file 
* @param rgb24path    Location of input RGB file. 
* @param width        Width of input RGB file. 
* @param height       Height of input RGB file. 
* @param url_out      Location of Output BMP file. 
*/
static int simplest_rgb24_to_bmp(const char *rgb24path,int width,int height,const char *bmppath)
{	
	typedef struct tagBITMAPFILEHEADER{ 
		unsigned short      bfType;       //位图文件的类型，一定为19778，其转化为十六进制为0x4d42，对应的字符串为BM
		unsigned int        bfSize;       //文件大小，以字节为单位 4字节 
		unsigned short      bfReserverd1; //位图文件保留字，2字节，必须为0   
		unsigned short      bfReserverd2; //位图文件保留字，2字节，必须为0
		unsigned int        bfbfOffBits;  //位图文件头到数据的偏移量，以字节为单位，4字节
	}BITMAPFILEHEADER;

    typedef struct tagBITMAPINFOHEADER{
		unsigned int  biSize;               //该结构大小，字节为单位,4字节
		int  biWidth;                       //图形宽度以象素为单位,4字节 
		int  biHeight;                      //图形高度以象素为单位，4字节
		unsigned short biPlanes;            //目标设备的级别，必须为1
		unsigned short biBitcount;          //颜色深度，每个象素所需要的位数, 一般是24
		unsigned int  biCompression;        //位图的压缩类型,一般为0,4字节
		unsigned int  biSizeImage;          //位图的大小，以字节为单位,4字节，即上面结构体中文件大小减去偏移(bfSize-bfOffBits)
		int  biXPelsPermeter;               //位图水平分辨率，每米像素数,一般为0 
		int  biYPelsPermeter;               //位图垂直分辨率，每米像素数,一般为0
		unsigned int  biClrUsed;            //位图实际使用的颜色表中的颜色数,一般为0
		unsigned int  biClrImportant;       //位图显示过程中重要的颜色数,一般为0
	}BITMAPINFOHEADER;

	int i=0,j=0;
	BITMAPFILEHEADER m_BMPHeader = {0};
	BITMAPINFOHEADER  m_BMPInfoHeader = {0};
	//位图第一部分，文件信息
	m_BMPHeader.bfType = (unsigned short)(('M' << 8) | 'B');
	m_BMPHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height *3*sizeof(char)-2*sizeof(char);//去掉结构体BITMAPFILEHEADER补齐的2字节
	m_BMPHeader.bfReserverd1 = 0;
	m_BMPHeader.bfReserverd2 = 0;
	m_BMPHeader.bfbfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)-2*sizeof(char);//真正的数据的位置
	
	unsigned char *rgb24_buffer = NULL;
	FILE *fp_rgb24 = NULL,*fp_bmp = NULL;
	
	if((fp_rgb24 = fopen(rgb24path,"r"))==NULL){
		printf("Error: Cannot open input RGB24 file.\n");
		return -1;
	}
	
	if((fp_bmp = fopen(bmppath,"w"))==NULL){
		printf("Error: Cannot open output BMP file.\n");
		return -1;
	}
	
	rgb24_buffer = (unsigned char *)malloc(width*height*3);
	fread(rgb24_buffer,1,width*height*3,fp_rgb24);

	//位图第二部分，数据信息
	m_BMPInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_BMPInfoHeader.biWidth = width;
	//BMP storage pixel data in opposite direction of Y-axis (from bottom to top).
	m_BMPInfoHeader.biHeight = -height;//BMP图片从最后一个点开始扫描，显示时图片是倒着的，所以用-height，这样图片就正了
    m_BMPInfoHeader.biPlanes = 1;
    m_BMPInfoHeader.biBitcount = 24;
    m_BMPInfoHeader.biCompression = 0;//不压缩
    m_BMPInfoHeader.biSizeImage = width * height * 3 * sizeof(char);
    m_BMPInfoHeader.biXPelsPermeter = 0;
	m_BMPInfoHeader.biYPelsPermeter = 0;
    m_BMPInfoHeader.biClrUsed = 0;
	m_BMPInfoHeader.biClrImportant = 0;


    fwrite(&m_BMPHeader,sizeof(m_BMPHeader.bfType)+sizeof(m_BMPHeader.bfSize)+sizeof(m_BMPHeader.bfReserverd1),1,fp_bmp); 
//	fwrite(&m_BMPHeader.bfType,sizeof(m_BMPHeader.bfType),1,fp_bmp);
//	fwrite(&m_BMPHeader.bfSize,sizeof(m_BMPHeader.bfSize),1,fp_bmp);
//	fwrite(&m_BMPHeader.bfReserverd1,sizeof(m_BMPHeader.bfReserverd1),1,fp_bmp);
	fwrite(&m_BMPHeader.bfReserverd2,sizeof(m_BMPHeader.bfReserverd2),1,fp_bmp);
	fwrite(&m_BMPHeader.bfbfOffBits,sizeof(m_BMPHeader.bfbfOffBits),1,fp_bmp);

    printf("====BMP文件头： %lu,  BMP图片头: %lu\n",sizeof(m_BMPHeader),sizeof(m_BMPInfoHeader));
	
	fwrite(&m_BMPInfoHeader,1,sizeof(m_BMPInfoHeader),fp_bmp);
	
	//BMP save R1|G1|B1,R2|G2|B2 as B1|G1|R1,B2|G2|R2
	//It saves pixel data in Little Endian
	//So we change 'R' and 'B'
	for(j =0;j<height;j++)
	{
		for(i=0;i<width;i++)
		{
			char temp = rgb24_buffer[(j*width+i)*3+2];
			rgb24_buffer[(j*width+i)*3+2] = rgb24_buffer[(j*width+i)*3+0];
			rgb24_buffer[(j*width+i)*3+0] = temp;
		}
	}
	
	fwrite(rgb24_buffer,3*width*height,1,fp_bmp);
	fclose(fp_rgb24);
	fclose(fp_bmp);
	free(rgb24_buffer);
	printf("Finish generate %s!\n",bmppath);

	return 0;
}

static unsigned char clip_value(unsigned char x,unsigned char min_val,unsigned char  max_val)
{
	if(x>max_val)
	{
		return max_val;
	}
	else if(x<min_val)
	{
		return min_val;
	}
	else
	{
		return x;
	}
}

//RGB24 to YUV420P
static bool RGB24_TO_YUV420(unsigned char *RgbBuf,int w,int h,unsigned char *yuvBuf)
{
	unsigned char*ptrY, *ptrU, *ptrV, *ptrRGB;
	memset(yuvBuf,0,w*h*3/2);
	ptrY = yuvBuf;
	ptrU = yuvBuf + w*h;
	ptrV = ptrU + (w*h*1/4);
	unsigned char y, u, v, r, g, b;
	
	for (int j = 0; j<h;j++)
	{
		ptrRGB = RgbBuf + w*j*3;
		for (int i = 0;i<w;i++)
		{
			r = *(ptrRGB++);
			g = *(ptrRGB++);
			b = *(ptrRGB++);
			y = (unsigned char)( ( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16;
			u = (unsigned char)( ( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
			v = (unsigned char)( ( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
			*(ptrY++) = clip_value(y,0,255);
			
			if (j%2 == 0 && i%2 == 0)
			{
				*(ptrU++) = clip_value(u,0,255);
			}
			else
			{
				if (i%2 == 0)
				{
					*(ptrV++) = clip_value(v,0,255);
				}
			}
		}
	}

	return true;
}

/** 
* Convert RGB24 file to YUV420P file 
* @param url_in  Location of Input RGB file. 
* @param w       Width of Input RGB file. 
* @param h       Height of Input RGB file. 
* @param num     Number of frames to process. 
* @param url_out Location of Output YUV file. 
*/
static int simplest_rgb24_to_yuv420(const char *url_in, int w, int h,int num,const char *url_out)
{
	FILE *fp = fopen(url_in,"r+");
	FILE *fp1 = fopen(url_out,"w+");
	
	unsigned char *pic_rgb24 = (unsigned char *)malloc(w*h*3);
	unsigned char *pic_yuv420 = (unsigned char *)malloc(w*h*3/2);

    for(int i=0;i<num;i++)
	{
		fread(pic_rgb24,1,w*h*3,fp);
		RGB24_TO_YUV420(pic_rgb24,w,h,pic_yuv420);
		fwrite(pic_yuv420,1,w*h*3/2,fp1);
	}
	   
	free(pic_rgb24);
	free(pic_yuv420);
	fclose(fp);
	fclose(fp1);

	return 0;
}

/** 
* Generate RGB24 colorbar. 
* @param width    Width of Output RGB file. 
* @param height   Height of Output RGB file. 
* @param url_out  Location of Output RGB file. 
*/
static int simplest_rgb24_colorbar(int width, int height,const char *url_out)
{
	unsigned char *data = NULL;
	int barwidth;
	char filename[100] = {0};
	FILE *fp = NULL;
	int i = 0,j = 0;
	
	data = (unsigned char *)malloc(width*height*3);
	barwidth = width/8;
	
	if((fp = fopen(url_out,"w+")) == NULL){
		printf("Error: Cannot create file!");
		return -1;
	}
	
	for(j=0;j<height;j++)
	{
		for(i=0;i<width;i++)
		{
			int barnum = i/barwidth;
			switch(barnum)
			{
				case 0:
				{
					data[(j*width+i)*3+0] = 255;
					data[(j*width+i)*3+1] = 255;
					data[(j*width+i)*3+2] = 255;
					break;  
				}
				case 1:
				{
					data[(j*width+i)*3+0] = 255;
					data[(j*width+i)*3+1] = 255;
					data[(j*width+i)*3+2] = 0;
					break;
				}
				case 2:
				{
					data[(j*width+i)*3+0] = 0;
					data[(j*width+i)*3+1] = 255;
					data[(j*width+i)*3+2] = 255;
					break;
				}
                case 3:
				{
					data[(j*width+i)*3+0] = 0;
					data[(j*width+i)*3+1] = 255;
					data[(j*width+i)*3+2] = 0;
					break;
				}
				case 4:
				{
					data[(j*width+i)*3+0] = 255;
					data[(j*width+i)*3+1] = 0;
					data[(j*width+i)*3+2] = 255;
					break;
				}
                case 5:
				{
					data[(j*width+i)*3+0] = 255;
					data[(j*width+i)*3+1] = 0;
					data[(j*width+i)*3+2] = 0;
					break;
				}
				case 6:
				{
					data[(j*width+i)*3+0] = 0;
					data[(j*width+i)*3+1] = 0;
					data[(j*width+i)*3+2] = 255;
					break;
				}
                case 7:
				{
					data[(j*width+i)*3+0] = 0;
					data[(j*width+i)*3+1] = 0;
					data[(j*width+i)*3+2] = 0;
					break;
				}
			}
		}
	}
	
	fwrite(data,width*height*3,1,fp);
	fclose(fp);
	free(data);
	
	return 0;
}

///////////////////////PCM音频采样数据处理////////////////////////////////////////////

/**
* Split Left and Right channel of 16LE PCM file.
* @param url  Location of PCM file.
*/
static int simplest_pcm16le_split(const char *url)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/pcm/output_l.pcm","w+");
	FILE *fp2 = fopen("out/pcm/output_r.pcm","w+");
	
	unsigned char *sample = (unsigned char *)malloc(4);
	
	while(!feof(fp))
	{
		fread(sample,1,4,fp);
		//L
		fwrite(sample,1,2,fp1);
		//R
		fwrite(sample+2,1,2,fp2);
	}
	
	free(sample);
	fclose(fp);
	fclose(fp1);
	fclose(fp2);
	
	return 0;
}


/**
* Halve volume of Left channel of 16LE PCM file
* @param url  Location of PCM file.
*/
static int simplest_pcm16le_halfvolumeleft(const char *url)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/pcm/output_halfleft.pcm","w+");

	int cnt = 0;
	unsigned char *sample = (unsigned char *)malloc(4);

	while(!feof(fp))
	{
		short *samplenum = NULL;
		fread(sample,1,4,fp);

		samplenum = (short *)sample;
		*samplenum = *samplenum/2;

		//L
		fwrite(sample,1,2,fp1);
		//R
		fwrite(sample+2,1,2,fp1);

		cnt++;
	}

	printf("%s: Sample Cnt:%d\n",__FUNCTION__,cnt);

	free(sample);
	fclose(fp);
	fclose(fp1);

	return 0;
}

/**
* Re-sample to double the speed of 16LE PCM file
* @param url  Location of PCM file.
*/
static int simplest_pcm16le_doublespeed(const char *url)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/pcm/output_doublespeed.pcm","w+");

	int cnt = 0;
	unsigned char *sample = (unsigned char *)malloc(4);

	while(!feof(fp))
	{
		fread(sample,1,4,fp);
		if(cnt%2 != 0)
		{
			//L
			fwrite(sample,1,2,fp1);
			//R
			fwrite(sample+2,1,2,fp1);
		}

		cnt++;
	}

	printf("%s: Sample Cnt:%d\n",__FUNCTION__,cnt);

	free(sample);
	fclose(fp);
	fclose(fp1);

	return 0;
}

/**
* Convert PCM-16 data to PCM-8 data.
* @param url  Location of PCM file.
*/
static int simplest_pcm16le_to_pcm8(const char *url)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/pcm/output_8.pcm","w+");

	int cnt = 0;
	unsigned char *sample = (unsigned char *)malloc(4);

	while(!feof(fp))
	{
		short *samplenum16 = NULL;
		char samplenum8 = 0;
		unsigned char samplenum8_u = 0;
		fread(sample,1,4,fp);
        //(-32768-32767)
		samplenum16 = (short *)sample;
		samplenum8 = (*samplenum16) >> 8;
		//(0-255)
		samplenum8_u = samplenum8 + 128;
		//L
		fwrite(&samplenum8_u,1,1,fp1);

		samplenum16 = (short *)(sample+2);
		samplenum8 = (*samplenum16) >> 8;
		samplenum8_u = samplenum8 + 128;
		//R
        fwrite(&samplenum8_u,1,1,fp1);
		cnt++;
	}

	printf("%s: Sample Cnt:%d\n",__FUNCTION__,cnt);

	free(sample);
	fclose(fp);
	fclose(fp1);

	return 0;
}

/**
* Cut a 16LE PCM single channel file.
* @param url        Location of PCM file.
* @param start_num  start point
* @param dur_num    how much point to cut
*/
static int simplest_pcm16le_cut_singlechannel(const char *url,int start_num,int dur_num)
{
	FILE *fp = fopen(url,"r+");
	FILE *fp1 = fopen("out/pcm/output_cut.pcm","w+");
	FILE *fp_stat = fopen("out/pcm/output_cut.txt","w+");

	unsigned char *sample = (unsigned char *)malloc(2);
	int cnt = 0;

	while(!feof(fp))
	{
		fread(sample,1,2,fp);
		if(cnt>start_num && cnt<=(start_num + dur_num))
		{
			fwrite(sample,1,2,fp1);
			short samplenum = sample[1];
			samplenum = samplenum*256;
			samplenum = samplenum + sample[0];
			fprintf(fp_stat,"%6d,",samplenum);

			if(cnt%10 == 0)
				fprintf(fp_stat,"\n");
		}

		cnt++;
	}

	free(sample);
	fclose(fp);
	fclose(fp1);
	fclose(fp_stat);

	return 0;
}

/**
* Convert PCM16LE raw data to WAVE format
* @param pcmpath      Input PCM file.
* @param channels     Channel number of PCM file.
* @param sample_rate  Sample rate of PCM file.
* @param wavepath     Output WAVE file.
*
*  WAVE文件是一种RIFF格式的文件。其基本块名称是“WAVE”，其中包含了两个子块“fmt”和“data”
*  从编程的角度简单说来就是由WAVE_HEADER、WAVE_FMT、WAVE_DATA、采样数据共4个部分组成
*  它的结构如下所示
*  WAVE_HEADER
*  WAVE_FMT
*  WAVE_DATA
*  PCM数据
*/
static int simplest_pcm16le_to_wave(const char *pcmpath,int channels,int sample_rate,const char *wavepath)
{
	typedef struct WAVE_HEADER{
		char           ChunkID[4];         //内容为"RIFF"
		unsigned int   ChunkSize;          //存储文件的字节数（不包含ChunkID和ChunkSize这8个字节）
		char           Format[4];          //内容为"WAVE"
	}WAVE_HEADER;

	typedef struct WAVE_FMT{
		char             Subchunk1ID[4];      //内容为"fmt "
		unsigned int     Subchunk1Size;       //存储该子块的字节数,为16（不含前面的Subchunk1ID和Subchunk1Size这8个字节
		unsigned short   AudioFormat;         //存储音频文件的编码格式，例如若为PCM则其存储值为1，若为其他非PCM格式的则有一定的压缩
		unsigned short   NumChannels;         //通道数，单通道(Mono)值为1，双通道(Stereo)值为2
		unsigned int     SampleRate;          //采样率，如8k，44.1k等
		unsigned int     ByteRate;            //每秒存储的字节数，其值=SampleRate * NumChannels * BitsPerSample/8
		unsigned short   BlockAlign;          //块对齐大小，其值=NumChannels * BitsPerSample/8
		unsigned short   BitsPerSample;       //每个采样点的bit数，一般为8,16,32等
	}WAVE_FMT;

	typedef struct WAVE_DATA{
		char         Subchunk2ID[4];       //内容为“data”
		unsigned int Subchunk2Size;        //PCM原始裸数据字节数
	}WAVE_DATA;

	if(channels==0 || sample_rate==0)
	{
		channels = 2;
		sample_rate = 44100;
	}

	int bits = 16;
	WAVE_HEADER pcmHEADER;
	WAVE_FMT    pcmFMT;
	WAVE_DATA   pcmDATA;

	short m_pcmData;
	FILE *fp,*fpout;

	fp = fopen(pcmpath, "r");
	if(fp == NULL)
	{
		printf("%s: open pcm file error\n",__FUNCTION__);
		return -1;
	}

	fpout = fopen(wavepath,"w+");
	if(fpout == NULL)
	{
		printf("%s: create wav file error\n",__FUNCTION__);
		return -1;
	}

	//WAVE_HEADER
	memcpy(pcmHEADER.ChunkID,"RIFF",strlen("RIFF"));
	memcpy(pcmHEADER.Format,"WAVE",strlen("WAVE"));
	fseek(fpout,sizeof(WAVE_HEADER),SEEK_SET);

	//WAVE_FMT
	pcmFMT.SampleRate = sample_rate;
	pcmFMT.NumChannels = channels;
	pcmFMT.BitsPerSample = bits;
	pcmFMT.ByteRate = pcmFMT.SampleRate * pcmFMT.NumChannels * pcmFMT.BitsPerSample / 8;
	memcpy(pcmFMT.Subchunk1ID,"fmt ",strlen("fmt "));
	pcmFMT.Subchunk1Size = 16;
	pcmFMT.BlockAlign = pcmFMT.NumChannels * pcmFMT.BitsPerSample / 8;
	pcmFMT.AudioFormat = 1;

	fwrite(&pcmFMT,sizeof(WAVE_FMT),1,fpout);

	//WAVE_DATA
	memcpy(pcmDATA.Subchunk2ID,"data",strlen("data"));
	pcmDATA.Subchunk2Size = 0;
	fseek(fpout,sizeof(WAVE_DATA),SEEK_CUR);

    fread(&m_pcmData,sizeof(short),1,fp);
	while(!feof(fp))
	{
		pcmDATA.Subchunk2Size += 2;
		fwrite(&m_pcmData,sizeof(short),1,fpout);
		fread(&m_pcmData,sizeof(short),1,fp);
	}

	pcmHEADER.ChunkSize = 36 + pcmDATA.Subchunk2Size;
	rewind(fpout);
	fwrite(&pcmHEADER,sizeof(WAVE_HEADER),1,fpout);
	fseek(fpout,sizeof(WAVE_FMT),SEEK_CUR);
	fwrite(&pcmDATA,sizeof(WAVE_DATA),1,fpout);

	fclose(fp);
	fclose(fpout);

	return 0;
}

//////////////////H.264视频码流解析/////////////////////////////////////////

typedef enum{
	NALU_TYPE_SLICE    = 1,      //不分区、非IDR的片
	NALU_TYPE_DPA      = 2,      //片分区A
	NALU_TYPE_DPB      = 3,      //片分区B
	NALU_TYPE_DPC      = 4,      //片分区C
	NALU_TYPE_IDR      = 5,      //IDR图像中的片
	NALU_TYPE_SEI      = 6,      //补充增强信息单元(SEI)
	NALU_TYPE_SPS      = 7,      //序列参数集 (SPS)
	NALU_TYPE_PPS      = 8,      //图像参数集 (PPS)
	NALU_TYPE_AUD      = 9,      //分界符
	NALU_TYPE_EOSEQ    = 10,     //序列结束
	NALU_TYPE_EOSTREAM = 11,     //碼流结束
	NALU_TYPE_FILL     = 12      //填充
}NaluType;

typedef enum{
	NALU_PRIORITY_DISPOSABLE = 0,
	NALU_PRIRITY_LOW         = 1,
	NALU_PRIORITY_HIGH       = 2,
	NALU_PRIORITY_HIGHEST    = 3
}NaluPriority;

typedef struct{
	int startcodeprefix_len;          //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
	unsigned int len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	unsigned int max_size;            //! Nal Unit Buffer size
	int forbidden_bit;                //! should be always FALSE
	int nal_reference_idc;            //! NALU_PRIORITY_xxxx
	int nal_unit_type;                //! NALU_TYPE_xxxx
	char *buf;                        //! contains the first byte followed by the EBSP
}NALU_t;

static FILE* h264bitstream = NULL;    //the bit stream file
static int info2 = 0, info3 = 0;

static int FindStartCode2(unsigned char *Buf)
{
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=1)
		return 0; //0x000001?
	else
		return 1;
}

static int FindStartCode3(unsigned char *Buf)
{
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=0 || Buf[3] !=1)
		return 0;//0x00000001?
	else
		return 1;
}

static int GetAnnexbNALU(NALU_t *nalu)
{
	int pos = 0;
	int StartCodeFound,rewind;
	unsigned char *Buf;

	if((Buf = (unsigned char*)calloc(nalu->max_size,sizeof(char))) == NULL)
		printf("%s: Could not allocate Buf memory\n",__FUNCTION__);

	nalu->startcodeprefix_len = 3;
	if(3 != fread(Buf,1,3,h264bitstream))
	{
		free(Buf);
		return 0;
	}

//先找3字节的startCode
	info2 = FindStartCode2(Buf);
	if(info2 != 1)    //不是3字节的StartCode
	{
		if(1 != fread(Buf+3,1,1,h264bitstream))
		{
			free(Buf);
			return 0;
		}
        //再找4字节的StartCode
		info3 = FindStartCode3(Buf);
		if(info3 != 1)
		{
            //未找到4字节的StartCode
			free(Buf);
			return -1;
		}
		else
		{
            //找到4字节的StartCode
			pos = 4;
			nalu->startcodeprefix_len = 4;
		}
	}
	else //找到3字节的StartCode
	{
		nalu->startcodeprefix_len = 3;
		pos = 3;
	}

    //printf("==========haoge=====info2: %d   info3: %d   pos: %d  startcodeprefix_len: %d\n",info2,info3,pos,nalu->startcodeprefix_len);

	StartCodeFound = 0;
	info2 = 0;
	info3 = 0;

    //找到StartCode后，查找相邻的下一个StartCode的位置
	while(!StartCodeFound)
	{
		if(feof(h264bitstream))
		{
			nalu->len = (pos-1)-nalu->startcodeprefix_len;
			memcpy(nalu->buf,&Buf[nalu->startcodeprefix_len],nalu->len);
			nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit
			nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
			nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
			free(Buf);
			return pos-1;
		}

		Buf[pos++] = fgetc(h264bitstream);
		info3 = FindStartCode3(&Buf[pos-4]);
		if(info3 != 1)
			info2 = FindStartCode2(&Buf[pos-3]);
		StartCodeFound = (info2 == 1 || info3 == 1);
	}

	// Here, we have found another start code and read length of startcode bytes more than we should
    // have.  Hence, go back in the file
    rewind = (info3 == 1)? -4 : -3;

    //将文件指针重新定位到刚找到的StartCode的位置上
    if(0 != fseek(h264bitstream,rewind,SEEK_CUR))
    {
		free(Buf);
		printf("%s: Cannot fseek in the bit stream file",__FUNCTION__);
	}

	// Here the Start code, the complete NALU, and the next start code is in the Buf.
    // The size of Buf is pos, pos+rewind are the number of bytes excluding the next
    // start code, and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code

	nalu->len = (pos+rewind)-nalu->startcodeprefix_len;//NALU的大小，包括一个字节NALU头和EBSP数据
	memcpy(nalu->buf,&Buf[nalu->startcodeprefix_len], nalu->len);//从StartCode之后开始拷贝NALU字节流数据
	nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit,提取NALU头中的forbidden_bit (禁止位)
	nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit，提取NALU头中的nal_reference_bit (优先级)
	nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit,提取NALU头中的nal_unit_type (NAL类型)
	free(Buf);

	return (pos+rewind);
}

/**
* Analysis H.264 Bitstream
* @param url Location of input H.264 bitstream file.
*/
static int simplest_h264_parser(const char *url)
{
	NALU_t *n;
	int buffersize = 100000;

	//FILE *myout=fopen("output_log.txt","w+");
	FILE *myout = stdout;
	h264bitstream = fopen(url, "r+");
	if(h264bitstream == NULL)
	{
		printf("%s: Open file error\n",__FUNCTION__);
		return 0;
	}

	n = (NALU_t*)calloc(1,sizeof(NALU_t));
	if(n == NULL)
	{
		printf("%s: Alloc NALU Error\n",__FUNCTION__);
		return 0;
	}

	n->max_size = buffersize;
	n->buf = (char*)calloc(buffersize,sizeof(char));
	if(n->buf == NULL)
	{
		free(n);
		printf("%s: AllocNALU: n->buf",__FUNCTION__);
		return 0;
	}

	int data_offset = 0;
	int nal_num = 0;
	printf("-----+-------- NALU Table ------+---------+\n");
	printf(" NUM |    POS  |    IDC |  TYPE |   LEN   |\n");
	printf("-----+---------+--------+-------+---------+\n");

	while(!feof(h264bitstream))
	{
		int data_lenth;
		data_lenth = GetAnnexbNALU(n);
		char type_str[20] = {0};

		switch(n->nal_unit_type)
		{
			case NALU_TYPE_SLICE:
				sprintf(type_str,"SLICE");
				break;
			case NALU_TYPE_DPA:
				sprintf(type_str,"DPA");
				break;
			case NALU_TYPE_DPB:
				sprintf(type_str,"DPB");
				break;
			case NALU_TYPE_DPC:
				sprintf(type_str,"DPC");
				break;
			case NALU_TYPE_IDR:
				sprintf(type_str,"IDR");
				break;
			case NALU_TYPE_SEI:
				sprintf(type_str,"SEI");
				break;
			case NALU_TYPE_SPS:
				sprintf(type_str,"SPS");
				break;
			case NALU_TYPE_PPS:
				sprintf(type_str,"PPS");
				break;
			case NALU_TYPE_AUD:
				sprintf(type_str,"AUD");
				break;
			case NALU_TYPE_EOSEQ:
				sprintf(type_str,"EOSEQ");
				break;
			case NALU_TYPE_EOSTREAM:
				sprintf(type_str,"EOSTREAM");
				break;
			case NALU_TYPE_FILL:
				sprintf(type_str,"FILL");
				break;
		}

		char idc_str[20] = {0};
		switch(n->nal_reference_idc >>5)
		{
			case NALU_PRIORITY_DISPOSABLE:
				sprintf(idc_str,"DISPOS");
				break;
			case NALU_PRIRITY_LOW:
				sprintf(idc_str,"LOW");
				break;
			case NALU_PRIORITY_HIGH:
				sprintf(idc_str,"HIGH");
				break;
			case NALU_PRIORITY_HIGHEST:
				sprintf(idc_str,"HIGHEST");
				break;
		}

		fprintf(myout,"%5d| %8d| %7s| %6s| %8d|\n",nal_num,data_offset,idc_str,type_str,n->len);
		data_offset = data_offset + data_lenth;
		nal_num++;
	}

	//Free
	if(n)
	{
		if(n->buf)
		{
			free(n->buf);
			n->buf = NULL;
		}
		free(n);
	}

	return 0;
}

//////////////////////AAC音频码流解析/////////////////////////////////////////////////////

/*
 * AAC音频格式分析
 * AAC音频格式有ADIF和ADTS：
 *
 * ADIF：Audio Data Interchange Format 音频数据交换格式。这种格式的特征是可以确定的找到这个音频数据的开始，不需进行在音频数据流中间开始的解码
 *       即它的解码必须在明确定义的开始处进行。故这种格式常用在磁盘文件中
 *
 * ADTS：Audio Data Transport Stream 音频数据传输流。这种格式的特征是它是一个有同步字的比特流，解码可以在这个流中任何位置开始。它的特征类似于mp3数据流格式
 *       简单说，ADTS可以在任意帧解码，也就是说它每一帧都有头信息。ADIF只有一个统一的头，所以必须得到所有的数据后解码。且这两种的header的格式也是不同的，
 *       目前一般编码后的和抽取出的都是ADTS格式的音频流。
 *
 * 语音系统对实时性要求较高，基本是这样一个流程，采集音频数据，本地编码，数据上传，服务器处理，数据下发，本地解码
 *
 * ADTS是帧序列，本身具备流特征，在音频流的传输与处理方面更加合适。
 *
 * ADTS帧结构：
 *      header
 *      body
 *
 *ADTS的头信息都是7个字节，ADTS帧首部结构：
 *   序号	域	                         长度（bits）	           说明
 *   1	  Syncword	                      12	               同步头 总是0xFFF, all bits must be 1，代表着一个ADTS帧的开始
 *   2	MPEG version	                  1	                   0 for MPEG-4, 1 for MPEG-2
 *   3	Layer	                          2	                   always 0
 *   4	Protection Absent	              1	                   et to 1 if there is no CRC and 0 if there is CRC
 *   5	Profile	                          2	                   the MPEG-4 Audio Object Type minus 1
 *   6	MPEG-4 Sampling Frequency Index	  4	                   MPEG-4 Sampling Frequency Index (15 is forbidden)
 *   7	Private Stream	                  1	                   set to 0 when encoding, ignore when decoding
 *   8	MPEG-4 Channel Configuration	  3	                   MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
 *   9	Originality	                      1	                   set to 0 when encoding, ignore when decoding
 *   10	Home	                          1	                   set to 0 when encoding, ignore when decoding
 *   11	Copyrighted Stream	              1	                   set to 0 when encoding, ignore when decoding
 *   12	Copyrighted Start	              1	                   set to 0 when encoding, ignore when decoding
 *   13	Frame Length	                 13	                   this value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame)
 *   14	Buffer Fullness	                 11	                   buffer fullness
 *   15	Number of AAC Frames	         2	                   number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
 *
*/
static int getADTSframe(unsigned char* buffer, int buf_size, unsigned char* data ,int* data_size)
{
	int size = 0;

	if(!buffer || !data || !data_size)
	{
		return -1;
	}

	while(1)
	{
		if(buf_size < 7) //读取的acc原始碼流字节数不能小于ADTS头
		{
			return -1;
		}

		//Sync words
		if((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0))
		{
			//找到ADTS帧后，根据ADTS帧头部结构提取Frame Length字段
			size |= ((buffer[3] & 0x03) <<11);     //high 2 bit
			size |= buffer[4]<<3;                  //middle 8 bit
			size |= ((buffer[5] & 0xe0)>>5);       //low 3bit
			break;
		}

		--buf_size;
		++buffer;
	}

	//读取的AAC碼流字节数如果小于一帧ADTS长度，则需要接着继续读取后面的数据，保证解析AAC碼流至少包含一个ADTS帧
	if(buf_size < size)
	{
		return 1;
	}

	memcpy(data,buffer,size);
	*data_size = size;

	return 0;
}

/*
 * AAC码流解析的步骤就是首先从码流中搜索0x0FFF，分离出ADTS frame；然后再分析ADTS frame的首部各个字段
 * 本文的程序即实现了上述的两个步骤
 */
static int simplest_aac_parser(const char *url)
{
	int data_size = 0;
	int size = 0;
	int cnt = 0;
	int offset = 0;

	//FILE *myout = fopen("output_log.txt","w+");
	FILE *myout = stdout;

	unsigned char *aacframe = (unsigned char *)malloc(1024*5);
	unsigned char *aacbuffer = (unsigned char *)malloc(1024*1024);

	FILE *ifile = fopen(url, "r");
	if(!ifile)
	{
		printf("%s: Open file error",__FUNCTION__);
		return -1;
	}

	printf("-----+----------- ADTS Frame Table ----------+-------------+\n");
	printf(" NUM | Profile | Frequency | Channel Configurations | Size |\n");
	printf("-----+---------+-----------+------------------------+------+\n");

	while(!feof(ifile))
	{
		data_size = fread(aacbuffer+offset,1,1024*1024-offset,ifile);
		unsigned char* input_data = aacbuffer;

		while(1)
		{
			int ret = getADTSframe(input_data,data_size,aacframe,&size);

			if(ret == -1)
			{
				break;
			}
			else if(ret == 1)
			{
				memcpy(aacbuffer,input_data,data_size);
				offset = data_size;
				break;
			}

			char profile_str[10] = {0};
			char frequence_str[10] = {0};
			char channel_config_str[100] = {0};

			//aacframe保存了一帧ADTS
			unsigned char profile = aacframe[2] & 0xC0;
			profile = profile >> 6;

			switch(profile)
			{
				case 0:
					sprintf(profile_str,"Main");
					break;
				case 1:
					sprintf(profile_str,"LC");
					break;
				case 2:
					sprintf(profile_str,"SSR");
					break;
				default:
					sprintf(profile_str,"unknown");
					break;
			}

			unsigned char sampling_frequency_index = aacframe[2] & 0x3C;
			sampling_frequency_index = sampling_frequency_index >> 2;

			switch(sampling_frequency_index)
			{
				case 0:
					sprintf(frequence_str,"96000Hz");
					break;
				case 1:
					sprintf(frequence_str,"88200Hz");
					break;
				case 2:
					sprintf(frequence_str,"64000Hz");
					break;
				case 3:
					sprintf(frequence_str,"48000Hz");
					break;
				case 4:
					sprintf(frequence_str,"44100Hz");
					break;
				case 5:
					sprintf(frequence_str,"32000Hz");
					break;
				case 6:
					sprintf(frequence_str,"24000Hz");
					break;
				case 7:
					sprintf(frequence_str,"22050Hz");
					break;
				case 8:
					sprintf(frequence_str,"16000Hz");
					break;
				case 9:
					sprintf(frequence_str,"12000Hz");
					break;
				case 10:
					sprintf(frequence_str,"11025Hz");
					break;
				case 11:
					sprintf(frequence_str,"8000Hz");
					break;
				case 12:
					sprintf(frequence_str,"7350Hz");
					break;
				default:
					sprintf(frequence_str,"unknown");
					break;
			}

			unsigned char channel_config_index = aacframe[2] & 0x01 << 2;
			channel_config_index = channel_config_index | ((aacframe[3] & 0xC0) >> 6);
            switch(channel_config_index)
			{
				case 0:
					sprintf(channel_config_str,"Defined in AOT Specifc Config");
					break;
				case 1:
					 sprintf(channel_config_str,"1 channel");
					 break;
				case 2:
					  sprintf(channel_config_str,"2 channels");
					  break;
				case 3:
					  sprintf(channel_config_str,"3 channels");
					  break;
				case 4:
					  sprintf(channel_config_str,"4 channels");
					  break;
				case 5:
					  sprintf(channel_config_str,"5 channels");
					  break;
				case 6:
					  sprintf(channel_config_str,"6 channels");
					  break;
				case 7:
					  sprintf(channel_config_str,"8 channels");
					  break;
			}

			fprintf(myout,"%5d| %8s|  %8s|  %8s| %5d|\n",cnt,profile_str ,frequence_str,channel_config_str,size);
			data_size -= size;
			input_data += size;
			cnt++;
		}
	}

	fclose(ifile);
	free(aacbuffer);
	free(aacframe);

	return 0;
}

////////////////////////////FLV封装格式解析///////////////////////////////////////////////////

/*
 * FLV封装格式是由一个FLV Header文件头和一个一个的Tag组成的。Tag中包含了音频数据以及视频数据。FLV的结构如下所示
 *  FLV Header
 *  TAG(OnMetaData)
 *  Tag(vedio) [H.264]
 *  Tag(vedio) [H.264]
 *  Tag(audio) [AAC]
 *  Tag(vedio) [H.264]
 *  ...
 *  FLV Header部分记录了flv的类型、版本等信息，是flv的开头,占9bytes。
 *  其中每个Tag前面还包含了Previous Tag Size字段，4字节，表示前面一个Tag的大小。Tag的类型可以是视频、音频和Script，
 *  每个Tag只能包含以上三种类型的数据中的一种，每个Tag由也是由两部分组成的：Tag Header和Tag Data。
 *  Tag Header里存放的是当前Tag的类型、数据区（Tag Data）长度等信息
 */
#define TAG_TYPE_SCRIPT 18
#define TAG_TYPE_AUDIO  8
#define TAG_TYPE_VIDEO  9

typedef unsigned char byte;
typedef unsigned int  uint;

//FLV Header结构定义
typedef struct{
	byte Signature[3];        //3 bytes	,为"FLV"
	byte Version;             //1 byte,一般为0x01
	byte Flags;               //1 byte ,倒数第一位是1表示有视频，倒数第三位是1表示有音频，其它位必须为0
	uint DataOffset;          //4 bytes	,整个header的长度，一般为9,大于9表示下面还有扩展信息
}FLV_HEADER;

//Tag Header结构定义
typedef struct{
	byte TagType;            //Tag类型,1 bytes,8：音频 9：视频  18：脚本 其他：保留
	byte DataSize[3];        //数据区长度,3 bytes ,表示该Tag Data部分的大小
	byte Timestamp[3];       //时间戳,3 bytes ,整数，单位是毫秒。对于脚本型的tag总是0
	uint Reserved;           //代表1 bytes时间戳扩展和3 bytes StreamsID。时间戳扩展将时间戳扩展为4bytes，代表高8位，很少用到。StreamsID总是0
}TAG_HEADER;

//reverse_bytes - turn a BigEndian byte array into a LittleEndian integer
static uint reverse_bytes(byte *p, char c)
{
	int r = 0;
	int i;
	for(i=0; i<c; i++)
		r |= ( *(p+i) << (((c-1)*8)-8*i));
	return r;
}

/**
* Analysis FLV file
* @param url Location of input FLV file.
*/
static int simplest_flv_parser(const char *url)
{
	double fileSize = 0;
	//whether output audio/video stream
	int output_a = 1; //控制开关，1 输出音频流  0 不输出音频流
	int output_v = 1; //控制开关，1 输出视频流  0 不输出视频流

	FILE *ifh = NULL,*vfh = NULL, *afh = NULL;

	//FILE *myout = fopen("output_log.txt","w+");
    FILE *myout = stdout;

	FLV_HEADER flv;
	TAG_HEADER tagheader;
	uint previoustagsize, previoustagsize_z = 0 ,previousVedioTagsize = 0;

	ifh = fopen(url, "r+");
	if(ifh == NULL)
	{
		printf("%s: Failed to open files!",__FUNCTION__);
		return -1;
	}

	//FLV file header
    fread((char *)&flv,1,sizeof(FLV_HEADER),ifh);

	fprintf(myout,"============== FLV Header ==============\n");
	fprintf(myout,"Signature:  0x %c %c %c\n",flv.Signature[0],flv.Signature[1],flv.Signature[2]);
	fprintf(myout,"Version:    0x %X\n",flv.Version);
	fprintf(myout,"Flags  :    0x %X\n",flv.Flags);
	fprintf(myout,"HeaderSize: 0x %X\n",flv.DataOffset);
	fprintf(myout,"========================================\n");

    //move the file pointer to the end of the header
    fseek(ifh, flv.DataOffset, SEEK_SET);

	//process each tag
	do{
		//读取4字节整数Previous Tag Size
		int tmp;
		fread((char *)&tmp,1,4,ifh);
		previoustagsize = reverse_bytes((byte *)&tmp, sizeof(tmp));
		//读取Tag Header
		fread((void *)&tagheader,sizeof(TAG_HEADER),1,ifh);

        int tagheader_datasize = tagheader.DataSize[0]*65536 + tagheader.DataSize[1]*256 + tagheader.DataSize[2];
		int tagheader_timestamp = tagheader.Timestamp[0]*65536 + tagheader.Timestamp[1]*256 + tagheader.Timestamp[2];

//		fprintf(myout,"previoustagsize: %d, TagType: %d, tagheader_datasize: %d, tagheader_timestamp :%d\n",previoustagsize,tagheader.TagType,tagheader_datasize,tagheader_timestamp);
		//由于读取Tag头时需要读取sizeof(TAG_HEADER)个字节,故需要将文件指针重新定位到Tag头末尾
		fseek(ifh, 11-sizeof(TAG_HEADER), SEEK_CUR);

		char tagtype_str[10];
		switch(tagheader.TagType)
		{
			case TAG_TYPE_AUDIO:
				sprintf(tagtype_str,"AUDIO");
				break;
			case TAG_TYPE_VIDEO:
				sprintf(tagtype_str,"VIDEO");
				break;
			case TAG_TYPE_SCRIPT:
				sprintf(tagtype_str,"SCRIPT");
				break;
			default:
				sprintf(tagtype_str,"UNKNOWN");
				break;
		}

		fprintf(myout,"[%6s] %6d %6d |",tagtype_str,tagheader_datasize,tagheader_timestamp);

		//if we are not past the end of file, process the tag
        if(feof(ifh))
			break;

		//process tag by type
		switch(tagheader.TagType)
		{
			case TAG_TYPE_AUDIO:
            {
				char audiotag_str[100] = {0};
				strcat(audiotag_str,"| ");
				char tagdata_first_byte;
				tagdata_first_byte = fgetc(ifh);  //提取音频参数信息
				int x = tagdata_first_byte & 0xF0;
				x = x >> 4;  //音频编码类型
				switch(x)
				{
					case 0:
						strcat(audiotag_str,"Linear PCM, platform endian");
						break;
					case 1:
						strcat(audiotag_str,"ADPCM");
						break;
					case 2:
						strcat(audiotag_str,"MP3");
						break;
					case 3:
						strcat(audiotag_str,"Linear PCM, little endian");
						break;
					case 4:
						strcat(audiotag_str,"Nellymoser 16-kHz mono");
						break;
					case 5:
						strcat(audiotag_str,"Nellymoser 8-kHz mono");
						break;
					case 6:
						strcat(audiotag_str,"Nellymoser");
						break;
					case 7:
						strcat(audiotag_str,"G.711 A-law logarithmic PCM");
						break;
					case 8:
						strcat(audiotag_str,"G.711 mu-law logarithmic PCM");
						break;
					case 9:
						strcat(audiotag_str,"reserved");
						break;
					case 10:
						strcat(audiotag_str,"AAC");
						break;
					case 11:
						strcat(audiotag_str,"Speex");
						break;
					case 14:
						strcat(audiotag_str,"MP3 8-Khz");
						break;
					case 15:
						strcat(audiotag_str,"Device-specific sound");
						break;
					default:
						strcat(audiotag_str,"UNKNOWN");
						break;
				}

				strcat(audiotag_str,"| ");
				x = tagdata_first_byte & 0x0C;  //音频采样率,FLV封装格式并不支持48KHz的采样率
				x = x >> 2;
				switch(x)
				{
					case 0:
						strcat(audiotag_str,"5.5-kHz");
						break;
					case 1:
						strcat(audiotag_str,"11-kHz");
						break;
					case 2:
						strcat(audiotag_str,"22-kHz");
						break;
					case 3:
						strcat(audiotag_str,"44-kHz");
						break;
					default:
						strcat(audiotag_str,"UNKNOWN");
						break;
				}

				strcat(audiotag_str,"| ");
				x = tagdata_first_byte & 0x02;   //音频采样精度
				x = x >> 1;
				switch(x)
				{
					case 0:
						strcat(audiotag_str,"8Bit");
						break;
					case 1:
						strcat(audiotag_str,"16Bit");
						break;
					default:
						strcat(audiotag_str,"UNKNOWN");
						break;
				}

				strcat(audiotag_str,"| ");
				x = tagdata_first_byte & 0x01;   //音频类型
				switch(x)
				{
					case 0:
						strcat(audiotag_str,"Mono");
						break;
					case 1:
						strcat(audiotag_str,"Stereo");
						break;
					default:
						strcat(audiotag_str,"UNKNOWN");
						break;
				}

			    fprintf(myout,"%s",audiotag_str);
				//if the output file hasn't been opened, open it.
                if(output_a != 0 && afh == NULL)
					afh = fopen("out/flv/output.mp3", "w");

				//TagData - First Byte Data
				int data_size = tagheader_datasize - 1;
				if(output_a != 0)  //输出音频流
				{
					//TagData -1
					for(int i=0; i<data_size; i++)
						fputc(fgetc(ifh),afh);
				}
				else   // 不输出音频流
				{
					if((long)fileSize <= ftell(ifh)+data_size)
						for(int i=0; i<data_size; i++)
							fgetc(ifh);
					else
						fseek(ifh,data_size,SEEK_CUR);
				}

				break;
            }
			case TAG_TYPE_VIDEO:
			{
				char videotag_str[100] = {0};
				strcat(videotag_str,"| ");
				char tagdata_first_byte;
				//视频Tag Data也用开始的第1个字节包含视频数据的参数信息，从第2个字节开始为视频流数据,其中第1个字节的前4位的数值表示帧类型
				//第1个字节的后4位的数值表示视频编码类型
				tagdata_first_byte = fgetc(ifh);
				int x = tagdata_first_byte & 0xF0;
				x = x >> 4;  //获取视频帧类型
				switch(x)
				{
					case 1: //keyframe（for AVC，a seekable frame）
						strcat(videotag_str,"keyframe");
						break;
					case 2: //inter frame（for AVC，a nonseekable frame)
						strcat(videotag_str,"inter frame");
						break;
					case 3: //disposable inter frame（H.263 only)
						strcat(videotag_str,"disposable inter frame");
						break;
					case 4: //generated keyframe（reserved for server use)
						strcat(videotag_str,"generated keyframe");
						break;
					case 5: //video info/command frame
						strcat(videotag_str,"video info/command frame");
						break;
					default:
						strcat(videotag_str,"UNKNOWN");
						break;
				}

				strcat(videotag_str,"| ");
				x = tagdata_first_byte & 0x0F;  //获取视频编码类型
				switch(x)
				{
					case 1:
						strcat(videotag_str,"JPEG (currently unused)");
						break;
					case 2:
						strcat(videotag_str,"Sorenson H.263");
						break;
					case 3:
						strcat(videotag_str,"Screen video");
						break;
					case 4:
						strcat(videotag_str,"On2 VP6");
						break;
					case 5:
						strcat(videotag_str,"On2 VP6 with alpha channel");
						break;
					case 6:
						strcat(videotag_str,"Screen video version 2");
						break;
					case 7:
						strcat(videotag_str,"AVC");
						break;
					default:
						strcat(videotag_str,"UNKNOWN");
						break;
				}

				fprintf(myout,"%s",videotag_str);
				fseek(ifh, -1, SEEK_CUR);
				//if the output file hasn't been opened, open it.
				//提取第一个视频Tag
				if(vfh == NULL && output_v != 0)
				{
					//write the flv header (reuse the original file's header) and first previoustagsize
					vfh = fopen("out/flv/output.flv", "w");
					fwrite((char *)&flv,1, sizeof(flv),vfh);
					//重新定位文件指针到FLV Header末尾
					fseek(vfh,flv.DataOffset-sizeof(flv),SEEK_CUR);
					fwrite((char *)&previoustagsize_z,1,sizeof(previoustagsize_z),vfh);
					//TagHeader
                    fwrite((char *)&tagheader,1, sizeof(tagheader),vfh);
                    //重新定位文件指针到Tag Header末尾
                    fseek(vfh,11-sizeof(tagheader),SEEK_CUR);
                    //TagData
                    for(int i = 0; i < tagheader_datasize; i++)
						fputc(fgetc(ifh),vfh);
					//记录本视频Tag的大小
					previousVedioTagsize = 11 + tagheader_datasize;
				}
				else if(output_v != 0)  //提取后续视频Tag
				{
					//Previous Tag Size
                    fwrite((char *)&previousVedioTagsize,1, sizeof(previousVedioTagsize),vfh);
                    int data_size = tagheader_datasize;
					//TagHeader
                    fwrite((char *)&tagheader,1, sizeof(tagheader),vfh);
					//重新定位文件指针到Tag Header末尾
                    fseek(vfh,11-sizeof(tagheader),SEEK_CUR);
					//TagData
					for(int i = 0; i < data_size; i++)
						fputc(fgetc(ifh),vfh);
					//记录本视频Tag的大小
					previousVedioTagsize = 11 + tagheader_datasize;
				}
				else  //不输出视频流
				{
		            if((long)fileSize <= ftell(ifh)+tagheader_datasize)
						for(int i = 0; i < tagheader_datasize; i++)
							fgetc(ifh);
					else
						fseek(ifh,tagheader_datasize,SEEK_CUR);
				}
			   break;
			}
			case TAG_TYPE_SCRIPT:
			{
				fprintf(myout,"\n============== Script Tag Data==============\n");
				char scripttag_str[100] = {0};
				char tagdata_first_byte;

				/*脚本TAG中AMF数据第一个byte为此数据的类型，类型有:
				 * AMF_NUMBER = 0×00 代表double类型.占8字节
				 * AMF_BOOLEAN = 0×01 代表bool类型,占1字节
				 * AMF_STRING = 0×02 代表string类型,紧接着后面的2个字节表示字符串UTF8长度len,接着后面len个字节表示字符串UTF8格式的内容.
				 * AMF_OBJECT = 0×03 代表Hashtable，内容由UTF8字符串作为Key，其他AMF类型作为Value，该对象由3个字节：00 00 09来表示结束.
				 * AMF_MOVIECLIP = 0×04	不可用
				 * AMF_NULL = 0×05 Null就是空对象，该对象只占用一个字节，那就是Null对象标识0x05
				 * AMF_UNDEFINED = 0x06 Undefined也是只占用一个字节0x06
				 * AMF_REFERENCE = 0×07
				 * AMF_ECMA_ARRAY = 0×08 相当于Hashtable，与0x03不同的是用4个bytes记录该Hashtable的大小.
				 * AMF_OBJECT_END = 0×09 表示object结束
				 * AMF_STRICT_ARRAY = 0x0a
				 * AMF_DATE = 0x0b
				 * AMF_LONG_STRING = 0x0c
				 * AMF_UNSUPPORTED = 0x0d
				 * AMF_RECORDSET = 0x0e
				 * AMF_XML_DOC = 0x0f
				 * AMF_TYPED_OBJECT = 0×10
				 * AMF_AVMPLUS = 0×11
				 * AMF_INVALID = 0xff
				 *
				 * rtmp协议中数据都是大端的，所以在放数据前都要将数据转成大端的形式。AMF数据采用 Big-Endian（大端模式），主机采用Little-Endian（小端模式)
				 * 大端Big-Endian
				 * 低地址存放最高有效位（MSB），即高位字节排放在内存的低地址端，低位字节排放在内存的高地址端.符合人脑逻辑，与计算机逻辑不同
				 *
				 * 网络字节序 Network Order:TCP/IP各层协议将字节序定义为Big-Endian，因此TCP/IP协议中使用的字节序通常称之为网络字节序.
				 * 主机序 Host Orader:它遵循Little-Endian规则。所以当两台主机之间要通过TCP/IP协议进行通信的时候就需要调用相应的函数进行主机序（Little-Endian）
				 * 和网络序（Big-Endian）的转换。
				 */

				//第一个AMF包：
				//第1个字节表示AMF包类型，一般总是0x02，表示字符串。第2-3个字节为UI16类型值，标识字符串的长度，一般总是0x000A（“onMetaData”长度)
				//后面字节为具体的字符串，一般总为“onMetaData”（6F,6E,4D,65,74,61,44,61,74,61)
				tagdata_first_byte = fgetc(ifh);
				if(tagdata_first_byte == 2)
				{
					int ScriptDataLen = 0;
					byte tmp[2];
					fread(tmp,1,2,ifh); //读取字符串长度
					ScriptDataLen = tmp[0] * 256 + tmp[1];
					byte Data[ScriptDataLen+1] = { 0 };
					fread(Data,1,ScriptDataLen,ifh); //读取字符串onMetaData
					sprintf(scripttag_str,"ScriptDataLen: %d,  ScriptDataValue: %s",ScriptDataLen,Data);
					fprintf(myout,"[%6s]\n",scripttag_str);
				}

				//第二个AMF包：
				//第1个字节表示AMF包类型，一般总是0x08，表示数组。第2-5个字节为UI32类型值，表示数组元素的个数。后面即为各数组元素的封装，数组元素为元素名称和值组成的对.
				tagdata_first_byte = fgetc(ifh);
				if(tagdata_first_byte == 8)
				{
					//读取ECMA array元素个数
					int num = 0;
					fread((char *)&num,1,4,ifh);
					num = reverse_bytes((byte *)&num, sizeof(num));
					fprintf(myout,"ECMA array elementNum: %d\n",num);
					char key_value[100] = {0};
					for(int i=0;i < num;i++)
					{
						//读取key字符串的长度
					    int KeyLen = 0;
						byte tmp[2];
					    fread(tmp,1,2,ifh); //读取字符串长度
					    KeyLen = tmp[0] * 256 + tmp[1];
					    char KeyString[KeyLen+1] = { 0 };
					    fread(KeyString,1,KeyLen,ifh); //读取Key字符串

					    fprintf(myout,"===KeyString: %s\n",KeyString);
					    if(!strcmp(KeyString,"duration") || !strcmp(KeyString,"width") || !strcmp(KeyString,"height")
						    || !strcmp(KeyString,"videodatarate") || !strcmp(KeyString,"framerate") || !strcmp(KeyString,"videocodecid")
							|| !strcmp(KeyString,"audiodatarate") || !strcmp(KeyString,"audiosamplerate") || !strcmp(KeyString,"audiosamplesize")
							|| !strcmp(KeyString,"stereo") || !strcmp(KeyString,"audiocodecid") || !strcmp(KeyString,"filesize"))
					    {
							//读取value类型
							byte value_type = fgetc(ifh);
							if(value_type == 0)
							{
								//Number，8字节
								//读取Key对应的Value
								byte value[8] = { 0 };
							    union DOUBLE{
									 double numValue;
									 long   value;
								};
								DOUBLE dataVal;
								fread(value,1,8,ifh);
					           // fprintf(myout,"===value[0]: %0X  value[1]: %0X, value[2]: %0X, value[3]: %0X, value[4]: %0X, value[5]: %0X, value[6]: %0X, value[7]: %0X\n",
							   //         value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7]);

								dataVal.value = ((long)value[0] << 56) | ((long)value[1] << 48) | ((long)value[2] << 40) |
									           ((long)value[3] << 32) | ((long)value[4] << 24) | ((long)value[5] << 16) | ((long)value[6] << 8) | value[7];
								if(!strcmp(KeyString,"duration"))//时长
									fprintf(myout,"duration: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"width"))//视频宽度
									fprintf(myout,"width: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"height"))//视频高度
									fprintf(myout,"height: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"videodatarate"))//视频码率
									fprintf(myout,"videodatarate: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"framerate"))//视频帧率
									fprintf(myout,"framerate: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"videocodecid"))//视频编码方式
									fprintf(myout,"videocodecid: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"audiodatarate"))//音频码率
									fprintf(myout,"audiodatarate: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"audiosamplerate"))//音频采样率
									fprintf(myout,"audiosamplerate: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"audiosamplesize"))//音频采样精度
									fprintf(myout,"audiosamplesize: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"audiocodecid"))//音频编码方式
									fprintf(myout,"audiocodecid: %.4lf\n",dataVal.numValue);
								else if(!strcmp(KeyString,"filesize")){//文件大小
									fprintf(myout,"filesize: %.4lf\n",dataVal.numValue);
									fileSize = dataVal.numValue;
					                fprintf(myout,"===0==当前位置: %lu,  ScriTag Size: %d\n",ftell(ifh),tagheader_datasize);
								}
							}
							else if(value_type == 1)
							{
								//Boolean ,1字节
								//读取Key对应的Value
								bool stereo = fgetc(ifh) != 0 ? true: false;
								fprintf(myout,"stereo: %s\n",stereo?"立体声" : "单声道");
							}
					    }
						else
						{
							//读取value类型
							byte value_type = fgetc(ifh);
							if(value_type == 0)
								fseek(ifh, 8, SEEK_CUR);
							else if(value_type == 1)
								fseek(ifh, 1, SEEK_CUR);
							else if(value_type == 2){
								int ValueLen = 0;
								byte tmp[2];
								fread(tmp,1,2,ifh); //读取字符串长度
                                ValueLen = tmp[0] * 256 + tmp[1];
								fseek(ifh,ValueLen,SEEK_CUR); //跳过该字符串
							}
						}
					}
				}

				//数组结束位，占3个字节 一定为 0x 00 00 09,跳过这三个字节，将当前文件指针定位到Script Tag末尾，以便开始读取下一个Tag
				if(ftell(ifh) != (tagheader_datasize+9+4+11))
					fseek(ifh, tagheader_datasize+24, SEEK_SET);
				fprintf(myout,"===1==当前位置: %lu,  ScriTag Size: %d   long: %lu,  double: %lu\n",ftell(ifh),tagheader_datasize,sizeof(long),sizeof(double));
			}
		}
		fprintf(myout,"\n");
	}while (!feof(ifh));

	fclose(ifh);
	if(vfh)
		fclose(vfh);
	if(afh)
		fclose(afh);

	return 0;
}


///////////////////////////////UDP-RTP协议解析////////////////////////////////////////////

/*
* [memo] FFmpeg stream Command:
* 推流UDP封装的MPEG-TS
* ffmpeg -re -i sintel.ts -f mpegts udp://127.0.0.1:8888
* 推流首先经过RTP封装，然后经过UDP封装的MPEG-TS
* ffmpeg -re -i sintel.ts -strict -2 -f rtp_mpegts udp://127.0.0.1:8888
*
* 测试步骤
* 1 执行a.out程序
* 2 采用ffmpeg -re -i sintel.ts -strict -2 -f rtp_mpegts udp://127.0.0.1:8888 进行推流，
*   因为代码中parse_rtp开关开启，为0则使用ffmpeg -re -i sintel.ts -f mpegts udp://127.0.0.1:8888 推流
*/
#define CPU_LITTLE_ENDIAN 1
//对于RTP协议头，由于大小端对结构体的位域也有影响，定义的时候要考虑大小端问题
typedef struct RTP_FIXED_HEADER
{
#ifdef CPU_LITTLE_ENDIAN
	/* byte 0 */
	unsigned char csrc_len:4;       /* CSRC计数器，占4位，指示CSRC 标识符的个数,expect 0 */
	unsigned char extension:1;      /* 扩展标志，占1位，如果X=1，则在RTP报头后跟有一个扩展报头,expect 1 */
	unsigned char padding:1;        /* 填充标志，占1位，如果P=1，则在该报文的尾部填充一个或多个额外的八位组，它们不是有效载荷的一部分,expect 0 */
	unsigned char version:2;        /*RTP协议的版本号，占2位, expect 2 */

	/* byte 1 */
	unsigned char payload:7;       //有效荷载类型，占7位，用于说明RTP报文中有效载荷的类型，如GSM音频、JPEM图像等,在流媒体中大部分是用来区分音频流和视频流的，这样便于客户端进行解析
	unsigned char marker:1;        /* 标记，占1位，不同的有效载荷有不同的含义，对于视频，标记一帧的结束；对于音频，标记会话的开始,expect 1 */
#else
	/* byte 0 */
	unsigned char version:2;
	unsigned char padding:1;
	unsigned char extension:1;
    unsigned char csrc_len:4;

	/* byte 1 */
	unsigned char marker:1;
	unsigned char payload:7;
#endif

	/* bytes 2, 3 */
	unsigned short seq_no;        //序列号,占16位，用于标识发送者所发送的RTP报文的序列号，每发送一个报文，序列号增1。这个字段当下层的承载协议用UDP的时候,
                                  //网络状况不好的时候可以用来检查丢包。同时出现网络抖动的情况可以用来对数据进行重新排序，序列号的初始值是随机的，同时音频包和视频包的sequence是分别记数的.
	/* bytes 4-7 */
	unsigned int timestamp;       //时戳,占32位，必须使用90 kHz 时钟频率。时戳反映了该RTP报文的第一个八位组的采样时刻。接收者使用时戳来计算延迟和延迟抖动，并进行同步控制.

	/* bytes 8-11 */
	unsigned int ssrc;           //同步信源(SSRC)标识符：占32位，用于标识同步信源。该标识符是随机选择的，参加同一视频会议的两个同步信源不能有相同的SSRC.

}RTP_FIXED_HEADER;

//针对MPEG2-TS协议包格式及头参数介绍如下:
// header + payload  ... header + payload
// 其中header + payload为一个TS包，占188字节。包头结构如下定义,包头占4字节，payload数据占184字节
//MPEG-TS包头结构
typedef struct MPEGTS_FIXED_HEADER
{
	unsigned sync_byte: 8;                        //同步字节,固定为0x47 ,表示后面的是一个TS分组,说明从这个字节后的188个字节都属于一个ts包
	unsigned transport_error_indicator: 1;        //错误指示信息
	unsigned payload_unit_start_indicator: 1;     //负载单元开始标志
	unsigned transport_priority: 1;               //传输优先级标志
	unsigned PID: 13;                             //packet ID号码，唯一的号码对应不同的包,PID是TS流中包的唯一标志，表示了这个ts包负载数据的类型，Packet Data中是什么内容，完全由PID来决定.
	unsigned transport_scrambling_control: 2;     //加密标志
	unsigned adaptation_field_control: 2;         //附加区域控制
	unsigned continuity_counter: 4;               //包递增计数器
} MPEGTS_FIXED_HEADER;

static int simplest_udp_parser(int port)
{
	int cnt = 0;
	//FILE *myout=fopen("output_log.txt","wb+");
	FILE *myout = stdout;

	FILE *fp1 = fopen("out/udp-rtp/output_dump.ts","w+");

	int serSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if(serSocket ==-1){
		fprintf(myout,"socket:%m\n");
		return -1;
	}
	fprintf(myout,"建立socket成功\n");

	struct sockaddr_in serAddr;
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(port);
	inet_aton("127.0.0.1",&serAddr.sin_addr);

	if(bind(serSocket,(struct sockaddr*)&serAddr, sizeof(serAddr)) == -1){
		fprintf(myout,"bind error: %m!\n");
		close(serSocket);
		return -1;
	}

	struct sockaddr_in remoteAddr;
	socklen_t nAddrLen = sizeof(remoteAddr);

	int parse_rtp = 1; //通过UDP推流RTP封装的MPEG-TS
	int parse_mpegts = 1;//MPEG-TS解析开关
	fprintf(myout,"Listening on port %d\n",port);

	char recvData[10000];

	while(1)
	{
		int pktsize = recvfrom(serSocket,recvData,sizeof(recvData),0,(struct sockaddr *)&remoteAddr,&nAddrLen);
		if(pktsize > 0)
		{
			fprintf(myout,"packet size:%d, 发送者IP: %s,端口: %hu\n",pktsize,inet_ntoa(remoteAddr.sin_addr),ntohs(remoteAddr.sin_port));

			//Parse RTP
			if(parse_rtp != 0)
			{
				char payload_str[50] = {0};
				RTP_FIXED_HEADER rtp_header;
				int rtp_header_size = sizeof(RTP_FIXED_HEADER);

				//RTP Header
				memcpy((void *)&rtp_header,recvData,rtp_header_size);

				//RFC3551
				char payload = rtp_header.payload;
				switch(payload)
				{
					case 0:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","PCMU","Audio","8khz","1");
						break;
					case 1:
					case 2:
						sprintf(payload_str,"media type:%s","Audio");
						break;
					case 3:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","GSM","Audio","8khz","1");
						break;
					case 4:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","G723","Audio","8khz","1");
						break;
					case 5:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","DVI4","Audio","8khz","1");
						break;
					case 6:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","DVI4","Audio","16khz","1");
						break;
					case 7:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","LPC","Audio","8khz","1");
						break;
					case 8:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","PCMA","Audio","8khz","1");
						break;
					case 9:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","G722","Audio","8khz","1");
						break;
					case 10:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","L16","Audio","44.1khz","2");
						break;
					case 11:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","L16","Audio","44.1khz","1");
						break;
					case 12:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","QCELP","Audio","8khz","1");
						break;
					case 13:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","CN","Audio","8khz","1");
						break;
					case 14:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","MPA","Audio","90khz","1");
						break;
					case 15:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","G728","Audio","8khz","1");
						break;
					case 16:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","DVI4","Audio","11.025khz","1");
						break;
					case 17:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","DVI4","Audio","22.05khz","1");
						break;
					case 18:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","G729","Audio","8khz","1");
						break;
					case 25:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s","CelB","Vedio","90khz");
						break;
					case 26:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s","JPEG","Vedio","90khz");
						break;
					case 31:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s","H261","Vedio","90khz");
						break;
					case 32:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s","MPV","Vedio","90khz");
						break;
					case 33:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s","MP2T","AV","90khz");
						break;
					case 34:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s","H263","Vedio","90khz");
						break;
					case 96:
						sprintf(payload_str,"encoding name: %s,media type: %s,clock rate:%s,channels:%s","PCMU","Audio","8khz","2");
						break;
				}

				unsigned int timestamp = ntohl(rtp_header.timestamp);
				unsigned int seq_no = ntohs(rtp_header.seq_no);
				fprintf(myout,"[RTP Pkt] %5d| %5s| %10u| %5d| %5d|\n",cnt,payload_str,timestamp,seq_no,pktsize);

				//RTP Data
				char *rtp_data = recvData + rtp_header_size;
				int rtp_data_size = pktsize - rtp_header_size;
				fwrite(rtp_data,rtp_data_size,1,fp1);

				//Parse MPEGTS
				if(parse_mpegts != 0 && payload == 33)
				{
					//暂时不对MPEG-TS流解析
					MPEGTS_FIXED_HEADER mpegts_header;
					for(int i=0;i<rtp_data_size;i=i+188)
					{
						//判断是否是一个TS包的开始
						if(rtp_data[i] != 0x47)
							break;
						//MPEGTS Header
						//memcpy((void *)&mpegts_header,rtp_data+i,sizeof(MPEGTS_FIXED_HEADER));
						fprintf(myout,"   [MPEGTS Pkt]\n");
					}
				}
			}
			else
			{
				fprintf(myout,"[UDP Pkt] %5d| %5d|\n",cnt,pktsize);
				fwrite(recvData,pktsize,1,fp1);
			}

			cnt++;
		}
	}

	close(serSocket);
	fclose(fp1);
	return 0;
}


int main(int argc, char* argv[])
{
	  simplest_yuv420_split("yuv420p/lena_256x256_yuv420p.yuv",256,256,1);
      simplest_yuv444_split("yuv444p/lena_256x256_yuv444p.yuv",256,256,1);
      simplest_yuv420_gray("yuv420p/lena_256x256_yuv420p.yuv",256,256,1);
      simplest_yuv420_halfy("yuv420p/lena_256x256_yuv420p.yuv",256,256,1);
      simplest_yuv420_border("yuv420p/lena_256x256_yuv420p.yuv",256,256,20,1);
      simplest_yuv420_graybar(640,360,0,255,10,"out/yuv420p/output_graybar_640x360.yuv");
	  simplest_yuv420_psnr("yuv420p/lena_256x256_yuv420p.yuv","yuv420p/lena_distort_256x256_yuv420p.yuv",256,256,1);
	  simplest_rgb24_split("rgb24/cie1931_500x500.rgb",500,500,1);
	  simplest_rgb24_to_bmp("rgb24/lena_256x256_rgb24.rgb",256,256,"out/rgb24/output_lena.bmp");
	  simplest_rgb24_to_yuv420("rgb24/lena_256x256_rgb24.rgb",256,256,1,"out/rgb24/output_lena_256x256_yuv420p.yuv");
	  simplest_rgb24_colorbar(640, 360,"rgb24/colorbar_640x360.rgb");

	  simplest_pcm16le_split("pcm/NocturneNo2inEflat_44.1k_s16le.pcm");
      simplest_pcm16le_halfvolumeleft("pcm/NocturneNo2inEflat_44.1k_s16le.pcm");
      simplest_pcm16le_doublespeed("pcm/NocturneNo2inEflat_44.1k_s16le.pcm");
      simplest_pcm16le_to_pcm8("pcm/NocturneNo2inEflat_44.1k_s16le.pcm");
      simplest_pcm16le_cut_singlechannel("pcm/drum.pcm",2360,120);
      simplest_pcm16le_to_wave("pcm/NocturneNo2inEflat_44.1k_s16le.pcm",2,44100,"out/pcm/output_nocturne.wav");

	  simplest_h264_parser("h264/sintel.h264");

	  simplest_aac_parser("aac/nocturne.aac");

      simplest_flv_parser("flv/cuc_ieschool.flv");

	  simplest_udp_parser(8888);

	return 0;
}


