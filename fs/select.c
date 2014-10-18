#define ROUND_UP(x,y) (((x)+(y)-1)/(y))
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

struct poll_table_entry {
	struct file * filp;//�ҵ�������ļ���
	wait_queue_t wait;//����������ڵ�
	wait_queue_head_t * wait_address;//�������������
};

/*һ����ҳΪ��λ��¼�ȴ��¼���ÿһ������Ԫ�ص�poll_table_entry�������ݼ�¼��һ�εȴ�"����"
�����¼�����ʱ����ν��׻ᱻ��������˵�ջء�ÿ�ʽ�����ϸ��¼�˽���˫������Ϣ������׷�١�
*/
struct poll_table_page {
	struct poll_table_page * next;//���ע���fd�Ƚ϶࣬1ҳ4K�Ų��£����ô���������
	struct poll_table_entry * entry;//���ָ����Զָ����һ��Ҫ���ӵĽ��׵��׵�ַ��
	struct poll_table_entry entries[0];//����ָ��
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
/*���ú���ָ��.ֵ��ע����ǣ�__pollwait����ָ����ܻ��ڱ���fd�Ĺ����б��ÿա���������:
��ϵͳ�ڱ���fd�����У�����ǰ���fd�Ѿ�������Ԥ�ڵ��¼������ǣ�
������Ȼ�ģ����ǲ���Ҫ��ȥ���õȴ��¼��ˣ���Ϊ���ѭ�����ǿ϶�����ȴ�˯�ߵģ����ǻ�ֱ�ӷ���.count != 0
��Ȼ�ˣ�Ϊ�˱�֤�����ԣ���Ȼ�м��������¼�����������֪�����õ��ˣ����ǣ�������ȻӦ�ñ�����Щfd
ȥ�ж���Щfd�ǲ������¼������Ǹ��������һ������ɣ�Ҳ��������֪�Լ�������ˣ�
�ͱ���߱���˵��Ҫ����__pollwait �� ����������˵�Ҳ�����poll_freewait���������˵���
*/
	init_poll_funcptr(&pwq->pt, __pollwait);
	pwq->error = 0;
	pwq->table = NULL;//��ʼ��
}

EXPORT_SYMBOL(poll_initwait);

void poll_freewait(struct poll_wqueues *pwq)
{
	struct poll_table_page * p = pwq->table;
	while (p) {
		struct poll_table_entry * entry;
		struct poll_table_page *old;

		entry = p->entry;
		do {//ÿ�ζ�Ҫ������һ�㣬��������3
			entry--;//�Ӻ���ǰ��һ�ʱ��Ƴ�����
			//���Ǹ��ļ�/socket����֮��Ĺ�ϵ�Ͽ�
			remove_wait_queue(entry->wait_address,&entry->wait);
			fput(entry->filp);//������__pollwait�ɵĺ��£��±��ͷ��ˣ����������ü���
		} while (entry > p->entries);
		old = p;
		p = p->next;//��һ��
		free_page((unsigned long) old);//�黹��һҳ
	}
}

EXPORT_SYMBOL(poll_freewait);

//__pollwait������������ã�
//��һ������Ϊ�����ĵ�ǰ�ļ���������
//wait_address�Ǹ��ļ��ĵȴ����еĵ�ַ
//_p ��ʵ�͵��ڱ������ĵ�ַ
void __pollwait(struct file *filp, wait_queue_head_t *wait_address, poll_table *_p)
{
//container_of���_p���ڵ�poll_wqueues���׵�ַ��Ȼ�󷵻أ��ں��о�����ôʹ�õ�
	struct poll_wqueues *p = container_of(_p, struct poll_wqueues, pt);
	struct poll_table_page *table = p->table;

	if (!table || POLL_TABLE_FULL(table)) {//���������ˣ�������һ��
		struct poll_table_page *new_table;
		//һ��һҳ
		new_table = (struct poll_table_page *) __get_free_page(GFP_KERNEL);
		if (!new_table) {
			p->error = -ENOMEM;
			__set_current_state(TASK_RUNNING);
			return;
		}
		//�ɴ˿�֪��entryָ��ҳ��poll_table_entry����β��
		new_table->entry = new_table->entries;
		//�ҵ����е�ͷ��
		new_table->next = table;
		p->table = new_table;
		table = new_table;
	}
//����Ĺ���: 
//1. ��poll_table_page������һ����¼����ʾ���ڵ�������ļ�filp������ȴ�����wait_address�ϡ�
//2. ������εȴ��¼����뵽�豸�ĵȴ�����wait_address�У�ͨ��list_head˫���������
	/* Add a new entry */
	{
		struct poll_table_entry * entry = table->entry;
		table->entry = entry+1;//β����������,ע�⣬�ƶ�����һ���ṹ����ʼλ��
	 	get_file(filp);//��Ϊ�ҵ��������棬�������������ü���
	 	entry->filp = filp;//��¼�ȴ����ļ�,socket
		entry->wait_address = wait_address;//��¼�����ĸ��ȴ�������
		init_waitqueue_entry(&entry->wait, current);//����������ڵȹ�
		add_wait_queue(wait_address,&entry->wait);//��2��������������������¼���ӵ�wait_address���б���
		//��ʾ�ȴ�wait_address�Ľ��̼�¼
	}
/*
����چ���һ�µȴ����У�ϵͳ�ڱ��ȴ��ߺ͵ȴ���2���涼��¼������¼����൱��2���˽�飬�ض�˫�����н��֤
�ȴ���:��¼�����"����"�ǵ���˭���棬���̺��Ƕ��� ���ȴ�����˭
���ȴ���: ��¼˭�������ϵȴ�������ָ�򱻵ȴ��ߵĶ�Ӧwait_queue_t�ṹ��
���������¼�����ʱ�����ȴ����ܹ��ҵ���Ӧ�Ľ��̣�Ȼ���������ȴ���Ҳ�ܹ����ʵ�ʱ���л���
������εȴ�������poll_freewait���������������飬������εȴ���
*/
}

#define FDS_IN(fds, n)		(fds->in + n)
#define FDS_OUT(fds, n)		(fds->out + n)
#define FDS_EX(fds, n)		(fds->ex + n)

#define POLLFD_PER_PAGE  ((PAGE_SIZE-sizeof(struct poll_list)) / sizeof(struct pollfd))
//sock_poll��socket�ƹ�����
/* No kernel lock held - perfect */
static unsigned int sock_poll(struct file *file, poll_table * wait)
{
	struct socket *sock;

	/*
	 *	We can't return errors to poll, so it's either yes or no. 
	 */
	sock = SOCKET_I(file->f_dentry->d_inode);
	return sock->ops->poll(file, sock, wait);//����tcp��ʵ�ʵ���tcp_poll
}

//tcp_poll��tcp.c�ƶ�����
/*
 *	Wait for a TCP event.
 *
 *	Note that we don't need to lock the socket, as the upper poll layers
 *	take care of normal races (between the test and the event) and we don't
 *	go look at any of the socket buffers directly.
 */
unsigned int tcp_poll(struct file *file, struct socket *sock, poll_table *wait)
{
/*poll������ѯ�����Ƿ����¼������ˣ���ע��һ���ȴ��¼���
���������ư���Ϊ��poll_wait���õ���ô�� � 
�����socket�������¼����Ǿͱ�ʾ����Ҫע���¼��ˣ���Ϊ���Ǵ����Ҫ���ص�
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
//����Ƿ����¼�
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

/*����һҳpollfd�����fdpageΪ��ҳ�ľ���ṹ��ʼ��ַ��
//pwait�ǻص�������װ���ĵ�ַ(ָ���ָ��),
//count����仯��Ŀ���ۼӣ��������㴦��
pwait������������е��Ժ�������һ��:
pwaitһ��ѭ�����ÿգ���ʾ���ظ�ע��ȴ��¼���
pwait����ѯ�����У���û�з������¼���fdע��һ���ȴ��¼���
�Թ��������ȫ��û�з����¼����������ȴ����¼����ѡ��������ѯ������
�������¼����������fd������Ҫע���¼��ˣ���Ϊ�������ǲ���ȴ��˵ġ�ps:�����ʱǰ��ķ������¼��� �
�����и���������ڷ����¼���Ϊɶ���ǲ���ǰ���ע��ȴ��¼���ȡ����???? Ϊ������?��Ϊ���Ƿ�����ȡ���ģ����˳���ʱ��
������Ϊ����ԭ��? ֪����ͬѧ���һ�� wuhaiwen@baidu.com

*/
static void do_pollfd(unsigned int num, struct pollfd * fdpage,
	poll_table ** pwait, int *count)
{
	int i;

	for (i = 0; i < num; i++) {//�Ա�ҳ������pollfd
		int fd;
		unsigned int mask;
		struct pollfd *fdp;

		mask = 0;
		fdp = fdpage+i;//ָ��ǰ��fd�ṹ
		fd = fdp->fd;//���
		if (fd >= 0) {//�Է���һ�ɣ�Ҫ���û�û�г�ʼ��������
		//������ݾ��ֵ���õ�file�ṹ����ʵ����Ϊsocket��file,�Ƚṹ��linuxʲô�����ļ�
			struct file * file = fget(fd);//�������ü���
			mask = POLLNVAL;
			if (file != NULL) {
				mask = DEFAULT_POLLMASK;
				//������豸��������֧�ֵȴ���ѯpoll.����ļ���socket����poll����������ʵ�ֵ�
				//����豸���ļ�ϵͳ�������ļ�ϵͳ����ʵ�֡�
				//����������tcp socketΪ���ӽ���һ����������ʵ�ֵ�poll.TCP��poll��Ա������af_inet.c�ļ��С�
				//poll�ֶα�����Ϊsock_poll,����ֱ�ӵ���tcp_poll,�ļ�ϵͳ��Ҳ���ƣ��ܵ�����pipe_poll�������ǵ�ʵ�ֻ�����ͬ:����poll_wait
				//poll_wait�豸���Լ��ĵȴ�����Ϊ����������pwait��ָ��Ļص�����������:__pollwait !!!
				if (file->f_op && file->f_op->poll)
					mask = file->f_op->poll(file, *pwait);
				mask &= fdp->events | POLLERR | POLLHUP;//�û�Ҫ��ע���¼�+���󣬶Ͽ��¼�����
				fput(file);//�������ü���
			}
			if (mask) {//�����0����ʾ�������û���ע���¼�fdp->events��POLLERR | POLLHUP
				*pwait = NULL;//���ص�������װ��ָ���ÿգ�ע�ⲻ�ǽ���װ���ÿգ��Ǹı������ָ��
				(*count)++;//out������¼�����˱仯���¼�
			}
		}
		fdp->revents = mask;//��¼���ļ�/socket�������¼����룬����Ӧ�ó����в�ѯ
	}
}

//nfds: ע���fd����
//list: �û���������
static int do_poll(unsigned int nfds,  struct poll_list *list,
			struct poll_wqueues *wait, long timeout)
{
	int count = 0;
	poll_table* pt = &wait->pt;//��¼�ص�����ָ��

	if (!timeout)//����ȴ�ʱ��Ϊ0,��������ʾ���ȴ�
		pt = NULL;
 
	for (;;) {//�����ƺ��Ĳ�����һֱ�ȴ���timeout
		struct poll_list *walk;
		set_current_state(TASK_INTERRUPTIBLE);//���жϵĵȴ�״̬�������жϻ���
		walk = list;//<��ͷ����>
		while(walk != NULL) {//һ�α���һҳ�����뱾ҳ��pollfd��Ŀ����ʼ��ַ���ص��������Լ��������
			//ptָ�����wait��pt�ĵ�ַ��������ӵ��ļ������˱仯����ᱻ�ÿ�poll_table,�Ӷ����ȴ�
			//�ֱ������ܲ�������������4
			do_pollfd( walk->len, walk->entries, &pt, &count);
			walk = walk->next;
		}
		pt = NULL;//ע���һ���¼��ˣ����ظ�ע��
		if (count || !timeout || signal_pending(current))//���¼�/��ʱ/�����ж�
			break;
		count = wait->error;
		if (count)//�д�
			break;
		timeout = schedule_timeout(timeout);//˯һ�ᣬȻ���ȥ˯����ʱ�丳ֵ��timeout��
		//����м����˽��ң���Ϊset_current_state(TASK_INTERRUPTIBLE);�������һᱻ���ѵ�
	}
	__set_current_state(TASK_RUNNING);//����ɶ�� ?���Ǳ�����������?˭������һ��hw_henry2008@126.com
	return count;
}

asmlinkage long sys_poll(struct pollfd __user * ufds, unsigned int nfds, long timeout)
{
	//�����������˵����poll��ȱ�ݵĸ�Դ���ڿ����룬
	//ÿ�ζ�Ҫ���ܶ��ظ��Ĺ����������ݣ������ڴ棬׼������
	//���ã�ʱ�临�Ӷ���O(n)��
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

	poll_initwait(&table);//����__pollwait����ָ�뵽table��pt����

	head = NULL;
	walk = NULL;
	i = nfds;
	err = -ENOMEM;
	while(i!=0) {//#������Ҫÿ�ζ��ظ�ͬ���Ĺ����������ڴ棬���û��ռ䵽�ں˿����ݡ���������1
		struct poll_list *pp;
		pp = kmalloc(sizeof(struct poll_list)+
				sizeof(struct pollfd)*
				(i>POLLFD_PER_PAGE?POLLFD_PER_PAGE:i),
					GFP_KERNEL);
		//���Ҫע���fd�Ƚ϶࣬һҳ�Ų��£�����Ҫѭ��һҳҳ���䣬���˫������
		//ÿ������Ԫ�ص�ǰ����listǰ��ָ��.ע��poll_list��һ���������飬entries��Ա����data���׵�ַ
		if(pp==NULL)
			goto out_fds;
		pp->next=NULL;
		pp->len = (i>POLLFD_PER_PAGE?POLLFD_PER_PAGE:i);//��¼��ҳ�ж��ٸ��û���������pollfd 
		if (head == NULL)
			head = pp;
		else
			walk->next = pp;

		walk = pp;//���û��ռ��п���pollfd �ṹ���ں˿ռ䣬�˴�������
		if (copy_from_user(pp->entries, ufds + nfds-i, 
				sizeof(struct pollfd)*pp->len)) {
			err = -EFAULT;
			goto out_fds;
		}
		i -= pp->len;
	}
	//����Ĳ��ֿ����û�ע���pollfd���鵽�ں˿ռ䣬׼��������
	fdcount = do_poll(nfds, head, &table, timeout);
	//�������������������������û��ռ䣬����ڴ�

	/* OK, now copy the revents fields back to user space. */
	walk = head;
	err = -EFAULT;
	//��������Ҫһ����������������2
	while(walk != NULL) {//һҳҳ�ı���ֱ��β����ע����Щҳ��˳��ʹ�������ufds����һһ��Ӧ
		struct pollfd *fds = walk->entries;//����ҳ�׵�ַ��ʼ��
		int j;

		for (j=0; j < walk->len; j++, ufds++) //��ÿһ��pollfd�������¼���д���û��ռ���
			if(__put_user(fds[j].revents, &ufds->revents))
				goto out_fds;
		}
		walk = walk->next;//����һҳ����
  	}
	err = fdcount;//��¼�����ı����Ŀ
	//��鵱ǰ�����Ƿ����źŴ������ز�Ϊ0��ʾ���ź���Ҫ����
	if (!fdcount && signal_pending(current))
		err = -EINTR;//��������ı����ĿΪ0���ҵ�ǰ���̷����жϣ�
		//��Ϊdo_poll��set_current_state(TASK_INTERRUPTIBLE)����������Ϊ���жϵĵȴ�״̬�ˣ�
		//���Կ��ܽ����ڵȴ������¼���ʱ�򣬷������жϣ�����������ǰ�˳��ȴ���
out_fds:
	walk = head;
	while(walk!=NULL) {//�黹�ڴ�
		struct poll_list *pp = walk->next;
		kfree(walk);
		walk = pp;
	}
	//!!!
	poll_freewait(&table);
	return err;
}
