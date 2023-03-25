/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup httpd Web server
 * @{
 * The uIP web server is a very simplistic implementation of an HTTP
 * server. It can serve web pages and files from a read-only ROM
 * filesystem, and provides a very small scripting language.

 */

/**
 * \file
 *         Web server
 * \author
 *         Adam Dunkels <adam@sics.se>
 */


/*
 * Copyright (c) 2004, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: httpd.c,v 1.2 2006/06/11 21:46:38 adam Exp $
 */

#include "uip.h"
#include "httpd.h"
#include "httpd-fs.h"
#include "httpd-cgi.h"
#include "http-strings.h"

#include <linux/string.h>


#define STATE_WAITING 0
#define STATE_OUTPUT  1
#define STATE_UPFILE  2
#define STATE_FINUP   3
#define STATE_ENDPOST 4

#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

#define HTTPD_INFO(fmt, args...)  \
	 //printf("[ %s ] %03d: "fmt"\n", __FUNCTION__, __LINE__, ##args)


#define UPFILE_BUFFER_ADDRESS 0x44000000

#define is_digit(c)	((c) >= '0' && (c) <= '9')

struct upfile_info
{
	int post_flag;
	int firstblock_flag;//是否收到了镜像文件的第一部分(即标识已经开始接受镜像文件)
	unsigned int content_count;//已收到的post请求正文字节数
	unsigned int content_len;//post请求的正文内容长度，不包含请求头
	unsigned int upfile_count;//已收到的上传文件的字节数,注:镜像升级文件的头4个
	                          //字节为镜像文件大小,upfile_count需与之一致.
	unsigned int upfile_boundary_len;//boundary字段字节数
	char upfile_boundary[128];//boundary字段内容
};

static struct upfile_info g_uinfo;
static char eol2[5] = { 0x0d, 0x0a, 0x0d, 0x0a, 0x00 };
static int g_update_retval = -1;


static int atoi(const char *s)
{	
	int i = 0;
	while(is_digit(*s))
	{
		i = i * 10 + *(s++) - '0';	
	}	

	return(i);
}

/*---------------------------------------------------------------------------*/
static unsigned short
generate_part_of_file(void *state)
{
  struct httpd_state *s = (struct httpd_state *)state;

  if(s->file.len > uip_mss()) {
    s->len = uip_mss();
  } else {
    s->len = s->file.len;
  }
  memcpy(uip_appdata, s->file.data, s->len);
  
  return s->len;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_file(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);
  
  do {
    PSOCK_GENERATOR_SEND(&s->sout, generate_part_of_file, s);
    s->file.len -= s->len;
    s->file.data += s->len;
  } while(s->file.len > 0);
      
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_part_of_file(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);

  PSOCK_SEND(&s->sout, s->file.data, s->len);
  
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static void
next_scriptstate(struct httpd_state *s)
{
  char *p;
  p = strchr(s->scriptptr, ISO_nl) + 1;
  s->scriptlen -= (unsigned short)(p - s->scriptptr);
  s->scriptptr = p;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_script(struct httpd_state *s))
{
  char *ptr;
  
  PT_BEGIN(&s->scriptpt);


  while(s->file.len > 0) {

    /* Check if we should start executing a script. */
    if(*s->file.data == ISO_percent &&
       *(s->file.data + 1) == ISO_bang) {
      s->scriptptr = s->file.data + 3;
      s->scriptlen = s->file.len - 3;
      if(*(s->scriptptr - 1) == ISO_colon) {
	httpd_fs_open(s->scriptptr + 1, &s->file);
	PT_WAIT_THREAD(&s->scriptpt, send_file(s));
      } else {
	PT_WAIT_THREAD(&s->scriptpt,
		       httpd_cgi(s->scriptptr)(s, s->scriptptr));
      }
      next_scriptstate(s);
      
      /* The script is over, so we reset the pointers and continue
	 sending the rest of the file. */
      s->file.data = s->scriptptr;
      s->file.len = s->scriptlen;
    } else {
      /* See if we find the start of script marker in the block of HTML
	 to be sent. */

      if(s->file.len > uip_mss()) {
	s->len = uip_mss();
      } else {
	s->len = s->file.len;
      }

      if(*s->file.data == ISO_percent) {
	ptr = strchr(s->file.data + 1, ISO_percent);
      } else {
	ptr = strchr(s->file.data, ISO_percent);
      }
      if(ptr != NULL &&
	 ptr != s->file.data) {
	s->len = (int)(ptr - s->file.data);
	if(s->len >= uip_mss()) {
	  s->len = uip_mss();
	}
      }
      PT_WAIT_THREAD(&s->scriptpt, send_part_of_file(s));
      s->file.data += s->len;
      s->file.len -= s->len;
      
    }
  }
  
  PT_END(&s->scriptpt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_headers(struct httpd_state *s, const char *statushdr))
{
  char *ptr;

  PSOCK_BEGIN(&s->sout);

  PSOCK_SEND_STR(&s->sout, statushdr);

  ptr = strrchr(s->filename, ISO_period);
  if(ptr == NULL) {
    PSOCK_SEND_STR(&s->sout, http_content_type_binary);
  } else if(strncmp(http_html, ptr, 5) == 0 ||
	    strncmp(http_shtml, ptr, 6) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_html);
  } else if(strncmp(http_css, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_css);
  } else if(strncmp(http_png, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_png);
  } else if(strncmp(http_gif, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_gif);
  } else if(strncmp(http_jpg, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_jpg);
  } else {
    PSOCK_SEND_STR(&s->sout, http_content_type_plain);
  }
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_output(struct httpd_state *s))
{
  char *ptr;
  
  PT_BEGIN(&s->outputpt);

  if(!httpd_fs_open(s->filename, &s->file)) {
    httpd_fs_open(http_404_html, &s->file);
    strcpy(s->filename, http_404_html);
    PT_WAIT_THREAD(&s->outputpt,
		   send_headers(s,
		   http_header_404));
    PT_WAIT_THREAD(&s->outputpt,
		   send_file(s));
  } else {
    PT_WAIT_THREAD(&s->outputpt,
		   send_headers(s,
		   http_header_200));
    ptr = strchr(s->filename, ISO_period);
    if(ptr != NULL && strncmp(ptr, http_shtml, 6) == 0) {
      PT_INIT(&s->scriptpt);
      PT_WAIT_THREAD(&s->outputpt, handle_script(s));
    } else {
      PT_WAIT_THREAD(&s->outputpt,
		     send_file(s));
    }
  }

  PSOCK_CLOSE(&s->sout);
  PT_END(&s->outputpt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_input(struct httpd_state *s))
{
	PSOCK_BEGIN(&s->sin);

	PSOCK_READTO(&s->sin, ISO_space);

	if((strncmp(s->inputbuf, http_get, 4) != 0) 
		&& (strncmp(s->inputbuf, http_post, 5) != 0)) 
	{
		PSOCK_CLOSE_EXIT(&s->sin);
	}

	if (strncmp(s->inputbuf, http_post, 5) == 0)
	{
		g_uinfo.post_flag = 1;
	}

	if (strncmp(s->inputbuf, http_get, 4) == 0)
	{
		g_uinfo.post_flag = 0;
	}
	
	PSOCK_READTO(&s->sin, ISO_space);
	
	if(s->inputbuf[0] != ISO_slash) {
	  PSOCK_CLOSE_EXIT(&s->sin);
	}

	if (1 == g_uinfo.post_flag)
	{
		strncpy(s->filename, http_index_html, sizeof(s->filename));
	}
	else
	{
		if(s->inputbuf[1] == ISO_space) {
		  strncpy(s->filename, http_index_html, sizeof(s->filename));
		} else {
		  s->inputbuf[PSOCK_DATALEN(&s->sin) - 1] = 0;
		  strncpy(s->filename, &s->inputbuf[0], sizeof(s->filename));
		}
		
		if (0 == strncmp(&s->inputbuf[0], "/reset", strlen("/reset")))
		{
			run_command("reset", 0);
		}
		
		if (0 == strncmp(&s->inputbuf[0], "/result.html", strlen("/result.html")))
		{
			if (0 == g_update_retval)
			{
				strncpy(s->filename, http_success_html, sizeof(s->filename));
			}
			else
			{
				strncpy(s->filename, http_fail_html, sizeof(s->filename));
			}		
		}
		
		/*	httpd_log_file(uip_conn->ripaddr, s->filename);*/

		s->state = STATE_OUTPUT;		
	}

	while(1)
	{
		PSOCK_READTO(&s->sin, ISO_nl);

		if(strncmp(s->inputbuf, http_referer, 8) == 0) {
		  HTTPD_INFO("Find http_referer\n");
		  printf("Find http_referer\n");
		  s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
		  /*	  httpd_log(&s->inputbuf[9]);*/
		}

		if (strncmp(s->inputbuf, http_content_len, 16) == 0)
		{
			s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
			g_uinfo.content_len = atoi(&s->inputbuf[16]);
			
			HTTPD_INFO("http_content_len is %s, %d, file len is %d\n", s->inputbuf, PSOCK_DATALEN(&s->sin), g_uinfo.content_len);
            printf("http_content_len is %s, %d, file len is %u\n", s->inputbuf, PSOCK_DATALEN(&s->sin), g_uinfo.content_len);
		}

		if (strncmp(s->inputbuf, http_content_type_multipart, 44) == 0)
		{
			if (PSOCK_DATALEN(&s->sin) == HTTPD_STATE_INPUT_BUFLEN - 1)
			{
				PSOCK_CLOSE_EXIT(&s->sin);
			}
			s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
            memset(g_uinfo.upfile_boundary, 0, sizeof(g_uinfo.upfile_boundary));
            printf("do memset\n");
			memcpy(g_uinfo.upfile_boundary+2, &s->inputbuf[44], PSOCK_DATALEN(&s->sin) - 2 - 44);
			g_uinfo.upfile_boundary[0] = '-';
			g_uinfo.upfile_boundary[1] = '-';
			g_uinfo.upfile_boundary_len = PSOCK_DATALEN(&s->sin) - 2 - 44 + 4; /* 4 means "--" and "\r\n" */
			
			//HTTPD_INFO("boundary is %s, len is %d\n", g_uinfo.upfile_boundary, g_uinfo.upfile_boundary_len);
            printf("boundary is %s, len is %u\n", g_uinfo.upfile_boundary, g_uinfo.upfile_boundary_len);
			s->state = STATE_UPFILE;
		}

		if (STATE_UPFILE == s->state)
		{
            if (0 == g_uinfo.firstblock_flag)
        	{
        	    /*
                此处处理的是content正文与http post请求头是未分开来发送即请求头所
                用的TCP报文中还包含content正文内容的情况;
                整个报文的格式如下:
                post请求头 + eol2字段 + boundary字段 + eol2字段 + 被传输文件的内容.
                注:包含传输文件内容所需最小字节数为768左右,正常tcp报文可处理，
                故暂不考虑eol2字段(标识正文开始)被分片处理的情况.
                */
        	    char *start = NULL;
                char *boundary_start = NULL;
                char *content_start = NULL;
                char find_first_block = 0;
        		if ((boundary_start = (char *)strstr((char *)uip_appdata, 
                    g_uinfo.upfile_boundary)) == NULL)
        		{
        			printf("no upfile_boundary\n");
        			return;
        		}

        		if ((start = (char *)strstr((char *)uip_appdata, eol2)) != NULL)
        		{
        		    printf("boundary_start:%p, start:%p \n", boundary_start, start);
        		    if (boundary_start == (start + 4))
                    {
                        start = (char *)strstr((char *)boundary_start, eol2);
                        if (start != NULL)
                        {
                            find_first_block = 1;
                            content_start = boundary_start;
                        }
                    }
                    else
                   {
                        find_first_block = 1;
                        content_start = start;
                    }
                }

                if (find_first_block)
                {
        			//HTTPD_INFO("find first block\n");
        			printf("find first block\n");
        			g_uinfo.firstblock_flag = 1;
        			g_uinfo.upfile_count = 0;
        			g_uinfo.content_count= 0;

        			/* skip 0d 0a 0d 0a */
        			start += 4;

        			g_uinfo.content_count += (unsigned int)(uip_len - (content_start - (char *)uip_appdata));
        			g_uinfo.upfile_count = (unsigned int)(uip_len - (start - (char *)uip_appdata));
        			memcpy((void *)UPFILE_BUFFER_ADDRESS, start, g_uinfo.upfile_count);

        			printf("handle_input:in first, count is %u, content count is %u,\n", g_uinfo.upfile_count, g_uinfo.content_count);
        		}
        	}
			if (strncmp(s->inputbuf, g_uinfo.upfile_boundary, strlen(g_uinfo.upfile_boundary)) == 0)
			{
				s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
				//HTTPD_INFO("Find boundary is %s, %d\n", s->inputbuf, PSOCK_DATALEN(&s->sin));
                printf("Find boundary is %s, %d\n", s->inputbuf, PSOCK_DATALEN(&s->sin));
			}
		}
	}
	
  PSOCK_END(&s->sin);
}

void handle_upfile(struct httpd_state *s)
{
	char *start = NULL;
	int retval = 0;

	HTTPD_INFO("in handle_upfile, package len is %d\n", uip_len);

	if (0 == uip_len)
	{
		return;
	}
	
	if (0 == g_uinfo.firstblock_flag)
	{
	    /*
	    此处处理的是content正文与http          post请求头是分开来发送即请求头使用一个tcp
	    报文发送;
	    content正文再使用新的tcp正文发送.其中content的报文头的格式是:
	    boundary字段 + eol2字段 + 被传输文件的内容.
	    */
		if (NULL == ((char *)strstr((char *)uip_appdata, g_uinfo.upfile_boundary)))
		{
			printf("no upfile_boundary\n");
			return;
		}

		if ((start = (char *)strstr((char *)uip_appdata, eol2)) != NULL)
		{
			//HTTPD_INFO("find first block\n");
			printf("find first block\n");
			g_uinfo.firstblock_flag = 1;
			g_uinfo.upfile_count = 0;
			g_uinfo.content_count= 0;

			/* skip 0d 0a 0d 0a */
			start += 4;

			g_uinfo.content_count += uip_len;
			g_uinfo.upfile_count = (unsigned int)(uip_len - (start - (char *)uip_appdata));
			memcpy((void *)UPFILE_BUFFER_ADDRESS, start, g_uinfo.upfile_count);

			
			HTTPD_INFO("in first, count is %d, content count is %d\n", g_uinfo.upfile_count, g_uinfo.content_count);
		}
	}
	else
	{
		/* normal block */
		g_uinfo.content_count += uip_len;

		if (g_uinfo.content_count < g_uinfo.content_len)
		{
			memcpy((void *)(UPFILE_BUFFER_ADDRESS + g_uinfo.upfile_count), uip_appdata, uip_len);
			g_uinfo.upfile_count += uip_len;
		}
		else if (g_uinfo.content_count == g_uinfo.content_len)
		{
			if (uip_len > (g_uinfo.upfile_boundary_len + 4))
			{
				memcpy((void *)(UPFILE_BUFFER_ADDRESS + g_uinfo.upfile_count), uip_appdata, (uip_len - g_uinfo.upfile_boundary_len - 4));
				g_uinfo.upfile_count += (uip_len - g_uinfo.upfile_boundary_len - 4); /* 4 means "--\r\n" */
			}
			else
			{
				printf("In small branch - %d.\n", uip_len);
				g_uinfo.upfile_count -= (g_uinfo.upfile_boundary_len + 4 - uip_len);
			}

			//HTTPD_INFO("in last, count is %d, content count is %d\n", g_uinfo.upfile_count, g_uinfo.content_count);
            printf("in last, count is %u, content count is %u\n", g_uinfo.upfile_count, g_uinfo.content_count);

			s->state = STATE_FINUP;
#if 0
			retval = nm_upgradeFirmware((char *)UPFILE_BUFFER_ADDRESS, g_uinfo.upfile_count);
			
			if (0 == retval)
			{
				strncpy(s->filename, http_success_html, sizeof(s->filename));
			}
			else
			{
				strncpy(s->filename, http_fail_html, sizeof(s->filename));
			}

			s->state = STATE_OUTPUT;
			memset(&g_uinfo, 0, sizeof(struct upfile_info));
#endif
		}
	}
}

void handle_update(struct httpd_state *s)
{
	int retval = 0;
	
	g_update_retval = nm_upgradeFirmware((char *)UPFILE_BUFFER_ADDRESS, g_uinfo.upfile_count);
	
	if (0 == g_update_retval)
	{
		strncpy(s->filename, http_success_html, sizeof(s->filename));
	}
	else
	{
		strncpy(s->filename, http_fail_html, sizeof(s->filename));
	}

	httpd_fs_open(s->filename, &s->file);
	
	s->state = STATE_OUTPUT;
	memset(&g_uinfo, 0, sizeof(struct upfile_info));

	return;
}


/*---------------------------------------------------------------------------*/
static void
handle_connection(struct httpd_state *s)
{
  HTTPD_INFO("handle_connection,package len is %d\n", uip_len);

  if (s->state == STATE_UPFILE){
    handle_upfile(s);
  }

  if (STATE_ENDPOST == s->state)
  {
    handle_update(s);
  }

  if (STATE_FINUP == s->state)
  {
  	s->state = STATE_ENDPOST;
  }
  else
  {
  handle_input(s);
  }

  if((s->state == STATE_OUTPUT)||(s->state == STATE_ENDPOST)) {
    handle_output(s);
  }
}
/*---------------------------------------------------------------------------*/
void
httpd_appcall(void)
{
  struct httpd_state *s = (struct httpd_state *)&(uip_conn->appstate);

  if(uip_closed() || uip_aborted() || uip_timedout()) {
  } else if(uip_connected()) {
    HTTPD_INFO("Phase 1\n");
    PSOCK_INIT(&s->sin, s->inputbuf, sizeof(s->inputbuf) - 1);
    PSOCK_INIT(&s->sout, s->inputbuf, sizeof(s->inputbuf) - 1);
    PT_INIT(&s->outputpt);
    s->state = STATE_WAITING;
    /*    timer_set(&s->timer, CLOCK_SECOND * 100);*/
    s->timer = 0;
    handle_connection(s);
  } else if(s != NULL) {
    HTTPD_INFO("Phase 2\n");
    if(uip_poll()) {
	  HTTPD_INFO("Phase 2.1\n");
      ++s->timer;
      if(s->timer >= 20) {
	  	printf("Phase 2.1-timeout\n");
		uip_abort();
      }
    } else {
      HTTPD_INFO("Phase 2.2\n");
      s->timer = 0;
    }
    handle_connection(s);
  } else {
    uip_abort();
  }
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Initialize the web server
 *
 *             This function initializes the web server and should be
 *             called at system boot-up.
 */
void
httpd_init(void)
{
  uip_listen(HTONS(80));
}
/*---------------------------------------------------------------------------*/
/** @} */
