#include "thread_pool.h"
#include <pthread.h>
#include "sys/time.h"
#include "stdlib.h"
#include <stdatomic.h>

#define SUCCESS 0;

struct thread_task
{
	thread_task_f function;
	void *arg;
	void *ret;

	/* PUT HERE OTHER MEMBERS */
	int is_active;
	int working_state; // 0 - created
					   // 1 - pushed
					   // 2 - working
					   // 3 - done
					   // 4 - joined
	int detach;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

struct thread_pool
{
	pthread_t *threads;

	/* PUT HERE OTHER MEMBERS */
	int is_active;
	int size;
	atomic_int working;
	int capacity;
	int tasks;
	int active_tasks;
	int task_idx;
	struct thread_task **queue;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

struct worker_arg
{
	struct thread_pool *pool;
};

// длинные названия(
void tmlp(struct thread_pool *p)
{
	pthread_mutex_lock(&(p->mutex));
}

void tmup(struct thread_pool *p)
{
	pthread_mutex_unlock(&(p->mutex));
}

void tmlt(struct thread_task *t)
{
	pthread_mutex_lock(&(t->mutex));
}

void tmut(struct thread_task *t)
{
	pthread_mutex_unlock(&(t->mutex));
}

// thead working function
void *thread_woker(void *worker_args)
{
	struct worker_arg *arg = worker_args;
	struct thread_pool *pool = arg->pool;
	// ждемс
	while (pool->is_active)
	{
		tmlp(pool);
		while (pool->active_tasks == 0 && pool->is_active)
		{
			pthread_cond_wait(&(pool->cond), &(pool->mutex));
			if (!pool->is_active)
			{
				tmup(pool);
				free(arg);
				return NULL;
			}
		}

		// pool lock
		struct thread_task *task = pool->queue[pool->task_idx];
		pool->queue[pool->task_idx] = NULL;
		pool->task_idx++;
		pool->task_idx %= TPOOL_MAX_TASKS;
		pool->working++;
		tmup(pool);

		if (task != NULL)
		{
			// task lock
			task->working_state = 2;
			task->is_active = 1;
			task->ret = task->function(task->arg);
			tmlt(task);
			task->working_state = 3;
			task->is_active = 0;
			tmlp(pool);
			pool->active_tasks--;
			tmup(pool);
			pthread_cond_broadcast(&(task->cond));

			if (task->detach)
			{
				tmut(task);
				thread_task_delete(task);
			}
			else
			{
				tmut(task);
			}
		}
		pool->working--;
	}

	free(arg);
	return NULL;
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count < 1)
	{
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	*pool = (struct thread_pool *)malloc(1 * sizeof(struct thread_pool));
	(*pool)->is_active = 1;
	(*pool)->size = 0;
	(*pool)->capacity = max_thread_count;
	(*pool)->tasks = 0;
	(*pool)->task_idx = 0;
	(*pool)->active_tasks = 0;
	(*pool)->working = 0;
	(*pool)->queue = (struct thread_task **)malloc(TPOOL_MAX_TASKS * sizeof(struct thread_task *));
	(*pool)->threads = (pthread_t *)malloc(max_thread_count * sizeof(pthread_t));
	pthread_mutex_init(&(*pool)->mutex, NULL);
	pthread_cond_init(&(*pool)->cond, NULL);
	return SUCCESS;
}

int thread_pool_thread_count_active(const struct thread_pool *pool)
{
	return pool->active_tasks;
}

int thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->size;
}

int thread_pool_delete(struct thread_pool *pool)
{
	tmlp(pool);
	if (pool->working > 0)
	{
		// printf("%d %d\n", pool->working, pool->size);
		tmup(pool);
		return TPOOL_ERR_HAS_TASKS;
	}
	pool->is_active = 0;
	tmup(pool);
	// printf("%d\n", thread_pool_thread_count_active(pool));
	// printf("%d %d\n", pool->tasks, pool->task_idx);
	// закрываем треды
	for (int i = 0; i < pool->size; i++)
	{
		pthread_cond_broadcast(&(pool->cond));
		pthread_join(pool->threads[i], NULL);
	}
	free(pool->threads);
	free(pool->queue);
	pthread_mutex_destroy(&(pool->mutex));
	pthread_cond_destroy(&(pool->cond));
	free(pool);
	return SUCCESS;
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	tmlp(pool);
	if (pool->active_tasks >= TPOOL_MAX_TASKS)
	{
		tmup(pool);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	tmlt(task);
	task->working_state = 1;
	task->is_active = 1;
	task->detach = 0;
	tmut(task);

	pool->queue[pool->tasks] = task;
	pool->tasks++;
	pool->tasks %= TPOOL_MAX_TASKS;
	pool->active_tasks++;

	if (pool->working < pool->size || pool->size >= pool->capacity)
	{
		pthread_cond_signal(&(pool->cond));
	}
	else
	{
		struct worker_arg *args = (struct worker_arg *)malloc(1 * sizeof(struct worker_arg));
		args->pool = pool;
		pthread_create(&(pool->threads[pool->size]), NULL, thread_woker, (void *)args);
		pool->size++;
	}
	tmup(pool);
	return SUCCESS;
}

int thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	*task = (struct thread_task *)malloc(1 * sizeof(struct thread_task));
	(*task)->function = function;
	(*task)->arg = arg;
	(*task)->is_active = 0;
	(*task)->working_state = 0;
	pthread_mutex_init(&(*task)->mutex, NULL);
	pthread_cond_init(&(*task)->cond, NULL);
	return SUCCESS;
}

bool thread_task_is_finished(const struct thread_task *task)
{
	return task->working_state == 3;
}

bool thread_task_is_running(const struct thread_task *task)
{
	return task->working_state == 2;
}

int thread_task_join(struct thread_task *task, void **result)
{
	tmlt(task);
	if (!task->is_active && task->working_state == 0)
	{
		tmut(task);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}

	if (task->working_state >= 3)
	{
		task->working_state = 4;
		*result = task->ret;
		tmut(task);
		return SUCCESS;
	}

	while (task->working_state < 3 && task->is_active)
	{
		pthread_cond_wait(&(task->cond), &(task->mutex));
	}

	task->working_state = 4;
	*result = task->ret;
	tmut(task);
	return SUCCESS;
}

#ifdef NEED_TIMED_JOIN

int thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	tmlt(task);
	if (!task->is_active && task->working_state == 0)
	{
		tmut(task);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}

	if (task->working_state >= 3)
	{
		task->working_state = 4;
		*result = task->ret;
		tmut(task);
		return SUCCESS;
	}

	// https://ibm.com/docs/en/i/7.4?topic=ssw_ibm_i_74/apis/users_77.html
	int wait_res = 0;
	int rc;
	struct timespec ts;
	struct timeval tp;
	rc = gettimeofday(&tp, NULL);

	/* Convert from timeval to timespec */
	ts.tv_sec = (long long int)tp.tv_sec + (long long int)timeout;
	ts.tv_nsec = (long long int)tp.tv_usec * 1000 + (long long int)((timeout - (long long int)timeout) * 1e9);

	if (ts.tv_nsec > 999999999)
	{
		ts.tv_sec += ts.tv_nsec / 1000000000;
		ts.tv_nsec = ts.tv_nsec % 1000000000;
	}

	while (task->is_active)
	{
		wait_res = pthread_cond_timedwait(&(task->cond), &(task->mutex), &ts);
		if (wait_res == 110 || !task->is_active)
			break;
	}

	if (task->is_active)
	{
		tmut(task);
		return TPOOL_ERR_TIMEOUT;
	}

	task->working_state = 4;
	*result = task->ret;
	tmut(task);
	return SUCCESS;
}

#endif

int thread_task_delete(struct thread_task *task)
{
	tmlt(task);
	
	if (task->working_state < 4 && task->working_state > 0 && !task->detach) {
		tmut(task);
		return TPOOL_ERR_TASK_IN_POOL;
	}
	tmut(task);
	pthread_mutex_destroy(&(task->mutex));
	pthread_cond_destroy(&(task->cond));
	free(task);
	return SUCCESS;
}

#ifdef NEED_DETACH

int thread_task_detach(struct thread_task *task)
{
	tmlt(task);
	if (!task->is_active && task->working_state == 0)
	{
		tmut(task);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}

	task->detach = 1;
	if (task->working_state == 3)
	{
		pthread_cond_broadcast(&(task->cond));
		tmut(task);
		thread_task_delete(task);
	}

	tmut(task);
	return SUCCESS;
}

#endif
