#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

uint8_t *buffer;
int buffer_length;
int width;
int height;

static int xioctl(int fd, int request, void *arg)
{
        int r;
 
        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);
 
        return r;
}

int init_caps(int fd)
{
	struct v4l2_format fmt = {0};
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
	{
		perror("Setting Pixel Format");
		return 1;
	}

	width = fmt.fmt.pix.width;
	height = fmt.fmt.pix.height;

	return 0;
}
 
int init_mmap(int fd)
{
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
 
    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        return 1;
    }
 
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
    {
        perror("Querying Buffer");
        return 1;
    }
 
    buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
	buffer_length = buf.length;
 
    return 0;
}
 
int capture_image(int fd, int index)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    {
        printf("Query Buffer\n");
        return 1;
    }
 
    if(-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        printf("Start Capture\n");
        return 1;
    }
 
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &tv);
    if(-1 == r)
    {
        printf("Waiting for Frame\n");
        return 1;
    }
 
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        printf("Retrieving Frame\n");
        return 1;
    }

    return 0;
}
 

unsigned char tmpBuf[] = {
0x42, 0x4D, 0x36, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 
0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int save2file(char *filename)
{
	FILE *fpO;

	fpO = fopen(filename, "wb");
	fwrite(tmpBuf, sizeof(tmpBuf), 1, fpO);
	for(int i=0; i<width*height; i++)
	{
		unsigned char buf[3];
		buf[0] = buffer[i];
		buf[1] = buffer[i];
		buf[2] = buffer[i];

		fwrite(buf, 3, 1, fpO);
	}

	fclose(fpO);

	return 0;
}

int main()
{
	int fd;
	int count = 0;
 
        fd = open("/dev/video0", O_RDWR);
        if (fd == -1)
        {
                perror("Opening video device");
                return 1;
        }

		if( init_caps(fd) )
            return 1;
        
        if(init_mmap(fd))
            return 1;
        int i;

        capture_image(fd, 0);
		save2file("output.bmp");

        close(fd);
        return 0;
}
