#define ROUND_UP(x,y) (((x)+(y)-1)/(y))
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

struct poll_table_entry {
	struct file * filp;//ÎÒµÈÔÚÕâ¸öÎÄ¼şÉÏ
	wait_queue_t wait;//ÊÇÕâ¸ö½ø³ÌÔÚµÈ
	wait_queue_head_t * wait_address;//µÈÔÚÕâ¸ö¶ÓÁĞÉÏ
};

/*Ò»°ãÒÔÒ³Îªµ¥Î»¼ÇÂ¼µÈ´ıÊÂ¼ş£¬Ã¿Ò»¸öÊı×éÔªËØµÄpoll_table_entryÀàĞÍÊı¾İ¼ÇÂ¼ÁËÒ»´ÎµÈ´ı"½»Ò×"
µ±ÓĞÊÂ¼ş·¢ÉúÊ±£¬Õâ´Î½»Ò×»á±»³·Ïú»òÕßËµÊÕ»Ø¡£Ã¿±Ê½»Ò×ÏêÏ¸¼ÇÂ¼ÁË½»Ò×Ë«·½µÄĞÅÏ¢¡£·½±ã×·×Ù¡£
*/
struct poll_table_page {
	struct poll_table_page * next;//Èç¹û×¢²áµÄfd±È½Ï¶à£¬1Ò³4K·Å²»ÏÂ£¬ÔòÓÃ´ËÁ´½ÓÆğÀ´
	struct poll_table_entry * entry;//Õâ¸öÖ¸ÕëÓÀÔ¶Ö¸ÏòÏÂÒ»¸öÒªÔö¼ÓµÄ½»Ò×µÄÊ×µØÖ·¡£
	struct poll_table_entry entries[0];//ÈáĞÔÖ¸Õë
};

#define POLL_TABLE_FULL(table) \
	((unsigned long)((table)->entry+1) > PAGE_SIZE + (unsigned long)(table))

/*
 * Ok, Peter made a complicated, but straightforward multiple_wait() function.
 * I have rewritten this, taking some shortcuts: This code may not be easy to
 * follow, but it should be free of race-conditions, and it's practical. If you
 * understand what I'm doing here, then you understand how the linux
 * sleep/wakeup mechanism works.
 *
 * Two very simple procedures, poll_wait() and poll_freewait() make all the
 * work.  poll_wait() is an inline-function defined in <linux/poll.h>,
 * as all select/poll functions have to call it to add an entry to the
 * poll table.
 */
void __pollwait(struct file *filp, wait_queue_head_t *wait_address, poll_table *p);

void poll_initwait(struct poll_wqueues *pwq)
{
/*ÉèÖÃº¯ÊıÖ¸Õë.ÖµµÃ×¢ÒâµÄÊÇ£¬__pollwaitº¯ÊıÖ¸Õë¿ÉÄÜ»áÔÚ±éÀúfdµÄ¹ı³ÌÖĞ±»ÖÃ¿Õ¡£ÀíÓÉÈçÏÂ:
µ±ÏµÍ³ÔÚ±éÀúfd¹ı³ÌÖĞ£¬·¢ÏÖÇ°ÃæµÄfdÒÑ¾­·¢ÉúÁËÔ¤ÆÚµÄÊÂ¼ş£¬ÓÚÊÇ£¬
ÀíËùµ±È»µÄ£¬ÎÒÃÇ²»±ØÒªÔÙÈ¥ÉèÖÃµÈ´ıÊÂ¼şÁË£¬ÒòÎªÕâ´ÎÑ­»·ÎÒÃÇ¿Ï¶¨²»»áµÈ´ıË¯ÃßµÄ£¬ÎÒÃÇ»áÖ±½Ó·µ»Ø.count != 0
µ±È»ÁË£¬ÎªÁË±£Ö¤ºÏÀíĞÔ£¬ËäÈ»ÖĞ¼äÅöµ½ÁËÊÂ¼ş·¢Éú£¬ÎÒÃÇÖªµÀ²»ÓÃµÈÁË£¬µ«ÊÇ£¬ÎÒÃÇÒÀÈ»Ó¦¸Ã±éÀúÕâĞ©fd
È¥ÅĞ¶ÏÕâĞ©fdÊÇ²»ÊÇÓĞÊÂ¼ş£¬ËãÊÇ¸øºóÃæµÄÈËÒ»¸ö»ú»á°É£¬Ò²ËãÊÇÄãÃ÷Öª×Ô¼º²»»áµÈÁË£¬
¾Í±ğ¸æËß±ğÈËËµÎÒÒªµÈÄã__pollwait £¬ ¶øºóÂíÉÏÓÖËµÎÒ²»µÈÁËpoll_freewait¡£ÕâÑù²»ÈËµÀ¡£
*/
	init_poll_funcptr(&pwq->pt, __pollwait);
	pwq->error = 0;
	pwq->table = NULL;//³õÊ¼»¯
}

EXPORT_SYMBOL(poll_initwait);

void poll_freewait(struct poll_wqueues *pwq)
{
	struct poll_table_page * p = pwq->table;
	while (p) {
		struct poll_table_entry * entry;
		struct poll_table_page *old;

		entry = p->entry;
		do {//Ã¿´Î¶¼ÒªÕû¸öÅÜÒ»±ã£¬ĞÔÄÜÎÊÌâ3
			entry--;//´ÓºóÍùÇ°£¬Ò»±Ê±ÊÒÆ³ı½»Ò×
			//°ÑÄÇ¸öÎÄ¼ş/socketºÍÎÒÖ®¼äµÄ¹ØÏµ¶Ï¿ª
			remove_wait_queue(entry->wait_address,&entry->wait);
			fput(entry->filp);//ÕâÊÇÔÚ__pollwait¸ÉµÄºÃÊÂ£¬ÅÂ±»ÊÍ·ÅÁË£¬¹ÊÔö¼ÓÒıÓÃ¼ÆÊı
		} while (entry > p->entries);
		old = p;
		p = p->next;//ÏÂÒ»¸ö
		free_page((unsigned long) old);//¹é»¹ÕâÒ»Ò³
	}
}

EXPORT_SYMBOL(poll_freewait);

//__pollwaitÓÉÇı¶¯³ÌĞòµ÷ÓÃ£¬
//µÚÒ»¸ö²ÎÊıÎªÇı¶¯µÄµ±Ç°ÎÄ¼şÃèÊö·û£¬
//wait_addressÊÇ¸ÃÎÄ¼şµÄµÈ´ı¶ÓÁĞµÄµØÖ·
//_p ÆäÊµ¾ÍµÈÓÚ±¾º¯ÊıµÄµØÖ·
void __pollwait(struct file *filp, wait_queue_head_t *wait_address, poll_table *_p)
{
//container_ofËã³ö_pËùÔÚµÄpoll_wqueuesµÄÊ×µØÖ·£¬È»ºó·µ»Ø£¬ÄÚºËÖĞ¾­³£ÕâÃ´Ê¹ÓÃµÄ
	struct poll_wqueues *p = container_of(_p, struct poll_wqueues, pt);
	struct poll_table_page *table = p->table;

	if (!table || POLL_TABLE_FULL(table)) {//Èç¹ûÕâ¸öÂúÁË£¬ÔÙÉêÇëÒ»¸ö
		struct poll_table_page *new_table;
		//Ò»´ÎÒ»Ò³
		new_table = (struct poll_table_page *) __get_free_page(GFP_KERNEL);
		if (!new_table) {
			p->error = -ENOMEM;
			__set_current_state(TASK_RUNNING);
			return;
		}
		//ÓÉ´Ë¿ÉÖª£¬entryÖ¸ÏòÒ³µÄpoll_table_entryÊı×éÎ²²¿
		new_table->entry = new_table->entries;
		//¹Òµ½¶ÓÁĞµÄÍ·²¿
		new_table->next = table;
		p->table = new_table;
		table = new_table;
	}
//ÏÂÃæµÄ¹¦ÄÜ: 
//1. ÔÚpoll_table_pageÖĞÔö¼ÓÒ»Ìõ¼ÇÂ¼£¬±íÊ¾ÎÒÔÚµÈ×ÅÕâ¸öÎÄ¼şfilpµÄÕâ¸öµÈ´ı¶ÓÁĞwait_addressÉÏ¡£
//2. °ÑÎÒÕâ´ÎµÈ´ıÊÂ¼ş¼ÓÈëµ½Éè±¸µÄµÈ´ı¶ÓÁĞwait_addressÖĞ£¬Í¨¹ılist_headË«ÏòÁ´±í¼ÓÈë
	/* Add a new entry */
	{
		struct poll_table_entry * entry = table->entry;
		table->entry = entry+1;//Î²²¿ÏòÉÏÔö³¤,×¢Òâ£¬ÒÆ¶¯µ½ÏÂÒ»¸ö½á¹¹ÌåÆğÊ¼Î»ÖÃ
	 	get_file(filp);//ÒòÎªÎÒµÈÔÚÄãÉÏÃæ£¬ËùÒÔÔö¼ÓÆäÒıÓÃ¼ÆÊı
	 	entry->filp = filp;//¼ÇÂ¼µÈ´ıµÄÎÄ¼ş,socket
		entry->wait_address = wait_address;//¼ÇÂ¼µÈÔÚÄÄ¸öµÈ´ı¶ÓÁĞÉÏ
		init_waitqueue_entry(&entry->wait, current);//ÊÇÕâ¸ö½ø³ÌÔÚµÈ¹ş
		add_wait_queue(wait_address,&entry->wait);//½«2ÕâÁ¬½ÓÆğÀ´£¬½«ÕâÌõ¼ÇÂ¼Ôö¼Óµ½wait_addressµÄÁĞ±íÖĞ
		//±íÊ¾µÈ´ıwait_addressµÄ½ø³Ì¼ÇÂ¼
	}
/*
´ó¸ÅÔÚ†ªàÂÒ»ÏÂµÈ´ı¶ÓÁĞ£¬ÏµÍ³ÔÚ±»µÈ´ıÕßºÍµÈ´ıÕß2·½Ãæ¶¼¼ÇÂ¼ÁËÕâ´ÎÊÂ¼ş£¬Ïàµ±ÓÚ2¸öÈË½á»é£¬±Ø¶¨Ë«·½¶¼ÓĞ½á»éÖ¤
µÈ´ıÕß:¼ÇÂ¼ÁËÕâ´Î"½»Ò×"ÊÇµÈÔÚË­ÉÏÃæ£¬½ø³ÌºÅÊÇ¶àÉÙ ±»µÈ´ıÕßÊÇË­
±»µÈ´ıÕß: ¼ÇÂ¼Ë­ÔÚÎÒÉíÉÏµÈ´ı£¬²¢ÇÒÖ¸Ïò±»µÈ´ıÕßµÄ¶ÔÓ¦wait_queue_t½á¹¹¡£
ÕâÑùµ±ÓĞÊÂ¼ş·¢ÉúÊ±£¬±»µÈ´ıÕßÄÜ¹»ÕÒµ½¶ÔÓ¦µÄ½ø³Ì£¬È»ºó»½ĞÑËü¡£µÈ´ıÕßÒ²ÄÜ¹»ÔÚÊÊµ±Ê±»úÓĞ»ú»á
³·ÏúÕâ´ÎµÈ´ı¡£±ÈÈçpoll_freewait¾ÍÊÇ×öÕâÑùµÄÊÂÇé£¬³·ÏúÕâ´ÎµÈ´ı¡£
*/
}

#define FDS_IN(fds, n)		(fds->in + n)
#define FDS_OUT(fds, n)		(fds->out + n)
#define FDS_EX(fds, n)		(fds->ex + n)

#define POLLFD_PER_PAGE  ((PAGE_SIZE-sizeof(struct poll_list)) / sizeof(struct pollfd))
//sock_poll´ÓsocketÒÆ¹ıÀ´µÄ
/* No kernel lock held - perfect */
static unsigned int sock_poll(struct file *file, poll_table * wait)
{
	struct socket *sock;

	/*
	 *	We can't return errors to poll, so it's either yes or no. 
	 */
	sock = SOCKET_I(file->f_dentry->d_inode);
	return sock->ops->poll(file, sock, wait);//¶ÔÓÚtcp£¬Êµ¼Êµ÷ÓÃtcp_poll
}

//tcp_poll´Ótcp.cÒÆ¶¯¹ıÀ´
/*
 *	Wait for a TCP event.
 *
 *	Note that we don't need to lock the socket, as the upper poll layers
 *	take care of normal races (between the test and the event) and we don't
 *	go look at any of the socket buffers directly.
 */
unsigned int tcp_poll(struct file *file, struct socket *sock, poll_table *wait)
{
/*pollÏòÇı¶¯Ñ¯ÎÊÆäÊÇ·ñÓĞÊÂ¼ş·¢ÉúÁË£¬²¢×¢²áÒ»¸öµÈ´ıÊÂ¼ş¡£
²»¹ıÎÒÄÉÃÆ°¡£¬ÎªºÎpoll_waitµ÷ÓÃµÄÄÇÃ´Ôç ¿ 
Èç¹û±¾socket·¢ÉúÁËÊÂ¼ş£¬ÄÇ¾Í±íÊ¾²»ĞèÒª×¢²áÊÂ¼şÁË£¬ÒòÎªÎÒÃÇ´ı»á¾ÍÒª·µ»ØµÄ
*/
	unsigned int mask;
	struct sock *sk = sock->sk;
	struct tcp_sock *tp = tcp_sk(sk);

	poll_wait(file, sk->sk_sleep, wait);
	if (sk->sk_state == TCP_LISTEN)
		return tcp_listen_poll(sk, wait);

	/* Socket is not locked. We are protected from async events
	   by poll logic and correct handling of state changes
	   made by another threads is impossible in any case.
	 */

	mask = 0;
	if (sk->sk_err)
		mask = POLLERR;

	/*
	 * POLLHUP is certainly not done right. But poll() doesn't
	 * have a notion of HUP in just one direction, and for a
	 * socket the read side is more interesting.
	 *
	 * Some poll() documentation says that POLLHUP is incompatible
	 * with the POLLOUT/POLLWR flags, so somebody should check this
	 * all. But careful, it tends to be safer to return too many
	 * bits than too few, and you can easily break real applications
	 * if you don't tell them that something has hung up!
	 *
	 * Check-me.
	 *
	 * Check number 1. POLLHUP is _UNMASKABLE_ event (see UNIX98 and
	 * our fs/select.c). It means that after we received EOF,
	 * poll always returns immediately, making impossible poll() on write()
	 * in state CLOSE_WAIT. One solution is evident --- to set POLLHUP
	 * if and only if shutdown has been made in both directions.
	 * Actually, it is interesting to look how Solaris and DUX
	 * solve this dilemma. I would prefer, if PULLHUP were maskable,
	 * then we could set it on SND_SHUTDOWN. BTW examples given
	 * in Stevens' books assume exactly this behaviour, it explains
	 * why PULLHUP is incompatible with POLLOUT.	--ANK
	 *
	 * NOTE. Check for TCP_CLOSE is added. The goal is to prevent
	 * blocking on fresh not-connected or disconnected socket. --ANK
	 */
	if (sk->sk_shutdown == SHUTDOWN_MASK || sk->sk_state == TCP_CLOSE)
		mask |= POLLHUP;
	if (sk->sk_shutdown & RCV_SHUTDOWN)
		mask |= POLLIN | POLLRDNORM;
//¼ì²éÊÇ·ñÓĞÊÂ¼ş
	/* Connected? */
	if ((1 << sk->sk_state) & ~(TCPF_SYN_SENT | TCPF_SYN_RECV)) {
		/* Potential race condition. If read of tp below will
		 * escape above sk->sk_state, we can be illegally awaken
		 * in SYN_* states. */
		if ((tp->rcv_nxt != tp->copied_seq) &&
		    (tp->urg_seq != tp->copied_seq ||
		     tp->rcv_nxt != tp->copied_seq + 1 ||
		     sock_flag(sk, SOCK_URGINLINE) || !tp->urg_data))
			mask |= POLLIN | POLLRDNORM;

		if (!(sk->sk_shutdown & SEND_SHUTDOWN)) {
			if (sk_stream_wspace(sk) >= sk_stream_min_wspace(sk)) {
				mask |= POLLOUT | POLLWRNORM;
			} else {  /* send SIGIO later */
				set_bit(SOCK_ASYNC_NOSPACE,
					&sk->sk_socket->flags);
				set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);

				/* Race breaker. If space is freed after
				 * wspace test but before the flags are set,
				 * IO signal will be lost.
				 */
				if (sk_stream_wspace(sk) >= sk_stream_min_wspace(sk))
					mask |= POLLOUT | POLLWRNORM;
			}
		}

		if (tp->urg_data & TCP_URG_VALID)
			mask |= POLLPRI;
	}
	return mask;
}

/*´¦ÀíÒ»Ò³pollfd¾ä±ú£¬fdpageÎª±¾Ò³µÄ¾ä±ú½á¹¹¿ªÊ¼µØÖ·£¬
//pwaitÊÇ»Øµ÷º¯Êı°ü×°Æ÷µÄµØÖ·(Ö¸ÕëµÄÖ¸Õë),
//countÊä³ö±ä»¯ÊıÄ¿£¬ÀÛ¼Ó£¬²»ÄÜÇåÁã´¦Àí
pwaitÕâ¸ö±äÁ¿»¹ÊÇÓĞµãÃÔºı£¬ÊáÀíÒ»ÏÂ:
pwaitÒ»´ÎÑ­»·ºóÖÃ¿Õ£¬±íÊ¾²»ÖØ¸´×¢²áµÈ´ıÊÂ¼ş¡£
pwaitÔÚÂÖÑ¯¹ı³ÌÖĞ£¬¶ÔÃ»ÓĞ·¢Éú¹ıÊÂ¼şµÄfd×¢²áÒ»¸öµÈ´ıÊÂ¼ş£¬
ÒÔ¹©´ı»áÈç¹ûÈ«¶¼Ã»ÓĞ·¢ÉúÊÂ¼ş£¬ÔòÓÃÀ´µÈ´ı±»ÊÂ¼ş»½ĞÑ¡£Èç¹ûÔÚÂÖÑ¯¹ı³ÌÖĞ
Óöµ½ÁËÊÂ¼ş£¬ÔòºóĞøµÄfd¶¼²»ĞèÒª×¢²áÊÂ¼şÁË£¬ÒòÎª·´ÕıÎÒÃÇ²»»áµÈ´ıÁËµÄ¡£ps:Èç¹û´ËÊ±Ç°ÃæµÄ·¢ÉúÁËÊÂ¼şÄØ ¿
²»¹ıÓĞ¸öÎÊÌâ¹ş£¬ÔÚ·¢ÏÖÊÂ¼şºó£¬ÎªÉ¶ÎÒÃÇ²»°ÑÇ°ÃæµÄ×¢²áµÈ´ıÊÂ¼ş¸øÈ¡Ïûµô???? ÎªÁËĞÔÄÜ?ÒòÎªÎÒÃÇ·´Õı»áÈ¡ÏûµÄ£¬µÈÍË³öµÄÊ±ºò¡£
»¹ÊÇÒòÎªÆäËûÔ­Òò? ÖªµÀµÄÍ¬Ñ§Çë½ÌÒ»ÏÂ wuhaiwen@baidu.com

*/
static void do_pollfd(unsigned int num, struct pollfd * fdpage,
	poll_table ** pwait, int *count)
{
	int i;

	for (i = 0; i < num; i++) {//¶Ô±¾Ò³µÄËùÓĞpollfd
		int fd;
		unsigned int mask;
		struct pollfd *fdp;

		mask = 0;
		fdp = fdpage+i;//Ö¸Ïòµ±Ç°µÄfd½á¹¹
		fd = fdp->fd;//¾ä±ú
		if (fd >= 0) {//ÒÔ·ÀÍòÒ»°É£¬ÒªÊÇÓÃ»§Ã»ÓĞ³õÊ¼»¯¡¤¡¤¡¤
		//ÏÂÃæ¸ù¾İ¾ä±úÖµ£¬µÃµ½file½á¹¹£¬ÆäÊµ¼ÊÉÏÎªsocket£¬file,µÈ½á¹¹£¬linuxÊ²Ã´¶¼ÊÇÎÄ¼ş
			struct file * file = fget(fd);//Ôö¼ÓÒıÓÃ¼ÆÊı
			mask = POLLNVAL;
			if (file != NULL) {
				mask = DEFAULT_POLLMASK;
				//Èç¹û´ËÉè±¸£¬Íø¿¨µÈÖ§³ÖµÈ´ı²éÑ¯poll.Èç¹ûÎÄ¼şÊÇsocket£¬ÔòpollÊÇÍø¿¨Çı¶¯ÊµÏÖµÄ
				//Èç¹ûÉè±¸ÊÇÎÄ¼şÏµÍ³£¬ÔòÓÉÎÄ¼şÏµÍ³Çı¶¯ÊµÏÖ¡£
				//ÏÂÃæÔÛÃÇÒÔtcp socketÎªÀı×Ó½éÉÜÒ»ÏÂÇı¶¯³ÌĞòÊµÏÖµÄpoll.TCPµÄpoll³ÉÔ±ÉèÖÃÔÚaf_inet.cÎÄ¼şÖĞ¡£
				//poll×Ö¶Î±»ÉèÖÃÎªsock_poll,ºóÕßÖ±½Óµ÷ÓÃtcp_poll,ÎÄ¼şÏµÍ³µÈÒ²ÀàËÆ£¬¹ÜµÀµ÷ÓÃpipe_poll£¬¶øËûÃÇµÄÊµÏÖ»ù±¾ÏàÍ¬:µ÷ÓÃpoll_wait
				//poll_waitÉè±¸ÒÔ×Ô¼ºµÄµÈ´ı¶ÓÁĞÎª²ÎÊı£¬µ÷ÓÃpwaitËùÖ¸ÏòµÄ»Øµ÷º¯Êı£¬¾ÍÊÇ:__pollwait !!!
				if (file->f_op && file->f_op->poll)
					mask = file->f_op->poll(file, *pwait);
				mask &= fdp->events | POLLERR | POLLHUP;//ÓÃ»§Òª¹Ø×¢µÄÊÂ¼ş+´íÎó£¬¶Ï¿ªÊÂ¼ş£¬Çó½»
				fput(file);//¼õÉÙÒıÓÃ¼ÆÊı
			}
			if (mask) {//Èç¹û·Ç0£¬±íÊ¾·¢ÉúÁËÓÃ»§¹Ø×¢µÄÊÂ¼şfdp->events»òPOLLERR | POLLHUP
				*pwait = NULL;//½«»Øµ÷º¯Êı°ü×°Æ÷Ö¸ÕëÖÃ¿Õ£¬×¢Òâ²»ÊÇ½«°ü×°Æ÷ÖÃ¿Õ£¬ÊÇ¸Ä±ä²ÎÊıµÄÖ¸Ïò
				(*count)++;//out²ÎÊı¼ÇÂ¼·¢ÉúÁË±ä»¯µÄÊÂ¼ş
			}
		}
		fdp->revents = mask;//¼ÇÂ¼´ËÎÄ¼ş/socket·¢ÉúµÄÊÂ¼şÑÚÂë£¬¿ÉÔÚÓ¦ÓÃ³ÌĞòÖĞ²éÑ¯
	}
}

//nfds: ×¢²áµÄfd×ÜÊı
//list: ÓÃ»§´«½øÀ´µÄ
static int do_poll(unsigned int nfds,  struct poll_list *list,
			struct poll_wqueues *wait, long timeout)
{
	int count = 0;
	poll_table* pt = &wait->pt;//¼ÇÂ¼»Øµ÷º¯ÊıÖ¸Õë

	if (!timeout)//Èç¹ûµÈ´ıÊ±¼äÎª0,¡¤¡¤£¬±íÊ¾²»µÈ´ı
		pt = NULL;
 
	for (;;) {//²»µ½»ÆºÓĞÄ²»ËÀ£¬Ò»Ö±µÈ´ıµ½timeout
		struct poll_list *walk;
		set_current_state(TASK_INTERRUPTIBLE);//¿ÉÖĞ¶ÏµÄµÈ´ı×´Ì¬£¬ÔÊĞí±»ÖĞ¶Ï»½ĞÑ
		walk = list;//<´ÓÍ·ÔÙÀ´>
		while(walk != NULL) {//Ò»´Î±éÀúÒ»Ò³£¬´«Èë±¾Ò³µÄpollfdÊıÄ¿£¬¿ªÊ¼µØÖ·£¬»Øµ÷º¯Êı£¬ÒÔ¼°Êä³ö²ÎÊı
			//ptÖ¸Ïò²ÎÊıwaitµÄptµÄµØÖ·£¬Èç¹û¼àÊÓµÄÎÄ¼ş·¢ÉúÁË±ä»¯£¬Ôò»á±»ÖÃ¿Õpoll_table,´Ó¶ø²»µÈ´ı
			//ÓÖ±éÀú£¬ÄÜ²»ÂıÂğ£¬ĞÔÄÜÎÊÌâ4
			do_pollfd( walk->len, walk->entries, &pt, &count);
			walk = walk->next;
		}
		pt = NULL;//×¢²á¹ıÒ»´ÎÊÂ¼şÁË£¬±ğÖØ¸´×¢²á
		if (count || !timeout || signal_pending(current))//ÓĞÊÂ¼ş/³¬Ê±/·¢ÉúÖĞ¶Ï
			break;
		count = wait->error;
		if (count)//ÓĞ´í
			break;
		timeout = schedule_timeout(timeout);//Ë¯Ò»»á£¬È»ºó¼õÈ¥Ë¯µôµÄÊ±¼ä¸³Öµ¸øtimeout¡£
		//Èç¹ûÖĞ¼äÓĞÈË½ĞÎÒ£¬ÒòÎªset_current_state(TASK_INTERRUPTIBLE);£¬ËùÒÔÎÒ»á±»½ĞĞÑµÄ
	}
	__set_current_state(TASK_RUNNING);//ÕâÊÇÉ¶Òâ ?²»ÊÇ±¾À´¾ÍÕâÑùÂğ?Ë­¸æËßÎÒÒ»ÏÂhw_henry2008@126.com
	return count;
}

asmlinkage long sys_poll(struct pollfd __user * ufds, unsigned int nfds, long timeout)
{
	//ºóÃæ»ù±¾¿ÉÒÔËµÃ÷£¬pollµÄÈ±ÏİµÄ¸ùÔ´ÔÚÓÚ¿ÉÖØÈë£¬
	//Ã¿´Î¶¼Òª×öºÜ¶àÖØ¸´µÄ¹¤×÷£¬¿¼Êı¾İ£¬·ÖÅäÄÚ´æ£¬×¼±¸Êı¾İ
	//»¹ºÃ£¬Ê±¼ä¸´ÔÓ¶ÈÊÇO(n)µÄ
	struct poll_wqueues table;
 	int fdcount, err;
 	unsigned int i;
	struct poll_list *head;
 	struct poll_list *walk;

	/* Do a sanity check on nfds ... */
	if (nfds > current->files->max_fdset && nfds > OPEN_MAX)
		return -EINVAL;

	if (timeout) {
		/* Careful about overflow in the intermediate values */
		if ((unsigned long) timeout < MAX_SCHEDULE_TIMEOUT / HZ)
			timeout = (unsigned long)(timeout*HZ+999)/1000+1;
		else /* Negative or overflow */
			timeout = MAX_SCHEDULE_TIMEOUT;
	}

	poll_initwait(&table);//ÉèÖÃ__pollwaitº¯ÊıÖ¸Õëµ½tableµÄptÀïÃæ

	head = NULL;
	walk = NULL;
	i = nfds;
	err = -ENOMEM;
	while(i!=0) {//#ÎÒÃÇĞèÒªÃ¿´Î¶¼ÖØ¸´Í¬ÑùµÄ¹¤×÷£¬¿ª±ÙÄÚ´æ£¬´ÓÓÃ»§¿Õ¼äµ½ÄÚºË¿¼Êı¾İ¡£ĞÔÄÜÎÊÌâ1
		struct poll_list *pp;
		pp = kmalloc(sizeof(struct poll_list)+
				sizeof(struct pollfd)*
				(i>POLLFD_PER_PAGE?POLLFD_PER_PAGE:i),
					GFP_KERNEL);
		//Èç¹ûÒª×¢²áµÄfd±È½Ï¶à£¬Ò»Ò³·Å²»ÏÂ£¬ÔòĞèÒªÑ­»·Ò»Ò³Ò³·ÖÅä£¬×é³ÉË«ÏòÁ´±í£¬
		//Ã¿¸öÁ´±íÔªËØµÄÇ°ÃæÊÇlistÇ°ºóÖ¸Õë.×¢Òâpoll_listÊÇÒ»¸öÈáĞÔÊı×é£¬entries³ÉÔ±¾ÍÊÇdataµÄÊ×µØÖ·
		if(pp==NULL)
			goto out_fds;
		pp->next=NULL;
		pp->len = (i>POLLFD_PER_PAGE?POLLFD_PER_PAGE:i);//¼ÇÂ¼±¾Ò³ÓĞ¶àÉÙ¸öÓÃ»§´«½øÀ´µÄpollfd 
		if (head == NULL)
			head = pp;
		else
			walk->next = pp;

		walk = pp;//´ÓÓÃ»§¿Õ¼äÖĞ¿½±´pollfd ½á¹¹µ½ÄÚºË¿Õ¼ä£¬´Ë´¦¿ªÏú´ó
		if (copy_from_user(pp->entries, ufds + nfds-i, 
				sizeof(struct pollfd)*pp->len)) {
			err = -EFAULT;
			goto out_fds;
		}
		i -= pp->len;
	}
	//ÉÏÃæµÄ²¿·Ö¿½±´ÓÃ»§×¢²áµÄpollfdÊı×éµ½ÄÚºË¿Õ¼ä£¬×¼±¸ºÃÁ´±í
	fdcount = do_poll(nfds, head, &table, timeout);
	//ÏÂÃæ½øĞĞÇåÀí¹¤×÷£¬¿½±´½á¹ûµ½ÓÃ»§¿Õ¼ä£¬Çå¿ÕÄÚ´æ

	/* OK, now copy the revents fields back to user space. */
	walk = head;
	err = -EFAULT;
	//ÏÂÃæÓĞĞèÒªÒ»±é±é±éÀú£¬ĞÔÄÜÎÊÌâ2
	while(walk != NULL) {//Ò»Ò³Ò³µÄ±éÀúÖ±µ½Î²²¿£¬×¢ÒâÕâĞ©Ò³µÄË³ĞòºÍ´«½øÀ´µÄufdsÊı×éÒ»Ò»¶ÔÓ¦
		struct pollfd *fds = walk->entries;//´ÓÕâÒ³Ê×µØÖ·¿ªÊ¼¡£
		int j;

		for (j=0; j < walk->len; j++, ufds++) //½«Ã¿Ò»¸öpollfd·¢ÉúµÄÊÂ¼ş¶¼Ğ´ÈëÓÃ»§¿Õ¼äÖĞ
			if(__put_user(fds[j].revents, &ufds->revents))
				goto out_fds;
		}
		walk = walk->next;//µ½ÏÂÒ»Ò³´¦Àí
  	}
	err = fdcount;//¼ÇÂ¼·¢Éú¸Ä±äµÄÊıÄ¿
	//¼ì²éµ±Ç°½ø³ÌÊÇ·ñÓĞĞÅºÅ´¦Àí£¬·µ»Ø²»Îª0±íÊ¾ÓĞĞÅºÅĞèÒª´¦Àí
	if (!fdcount && signal_pending(current))
		err = -EINTR;//Èç¹û·¢Éú¸Ä±äµÄÊıÄ¿Îª0£¬ÇÒµ±Ç°½ø³Ì·¢ÉúÖĞ¶Ï£¬
		//ÒòÎªdo_pollÖĞset_current_state(TASK_INTERRUPTIBLE)½«½ø³ÌÉèÖÃÎª¿ÉÖĞ¶ÏµÄµÈ´ı×´Ì¬ÁË£¬
		//ËùÒÔ¿ÉÄÜ½ø³ÌÔÚµÈ´ıÍøÂçÊÂ¼şµÄÊ±ºò£¬·¢ÉúÁËÖĞ¶Ï£¬ÕâÑù½ø³ÌÌáÇ°ÍË³öµÈ´ıÁË
out_fds:
	walk = head;
	while(walk!=NULL) {//¹é»¹ÄÚ´æ
		struct poll_list *pp = walk->next;
		kfree(walk);
		walk = pp;
	}
	//!!!
	poll_freewait(&table);
	return err;
}
