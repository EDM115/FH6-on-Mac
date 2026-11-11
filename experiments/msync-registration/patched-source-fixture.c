/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Exported from the October 2026 FH6 experiments, see THIRD_PARTY_NOTICES.md
 */
/*
 * mach semaphore-based synchronization objects
 *
 * Copyright (C) 2018 Zebediah Figura
 * Copyright (C) 2023 Marc-Aurel Zent
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int NTSTATUS;
typedef uint64_t ULONGLONG;
typedef int mach_msg_return_t;
typedef struct { unsigned int msgh_remote_port, msgh_bits, msgh_id, msgh_size; } mach_msg_header_t;
typedef struct { mach_msg_header_t header; unsigned int shm_idx[65]; } mach_register_message_t;
struct event { int value, unused; unsigned short msync_type, refcount; int multiple_waiters; };
struct mutex { int tid, count; unsigned short msync_type, refcount; int multiple_waiters; };
#define MACH_MSG_SUCCESS 0
#define MACH_SEND_MSG 1
#define MACH_PORT_NULL 0
#define MACH_MSG_TIMEOUT_NONE 0
#define MSYNC_MUTEX 9
#define STATUS_PENDING 259
#define STATUS_TIMEOUT 258
#define STATUS_SUCCESS 0
#define UL_COMPARE_AND_WAIT_SHARED 3
#define ULF_NO_ERRNO 0
#define ERR(...) ((void)0)
static int tid_storage[2], *shm_tid_map=tid_storage, server_port, msgh_bits_send, removals, failures;
static int mach_msg2(mach_msg_header_t *header, int a, int b, int c, int d, int e, int f) {
    mach_register_message_t *message=(void *)header;
    assert(message->shm_idx[0] & (1u<<29)); ++removals; return 0;
}
static uint64_t update_timeout(uint64_t end) { return end; }
static int ulock_wait(int a, int *b, int c, uint64_t d) { *b=0; return 0; }
static int server_register_wait(unsigned int id, const int *objs, void **shm, int alert, void *alert_shm, int count) {
    for(int i=0;i<count;i++) ((struct event *)shm[i])->multiple_waiters++;
    if(alert) ((struct event *)alert_shm)->multiple_waiters++;
    return 0;
}
static void check(int pass, const char *name) { if(!pass) { printf("FAIL %s\n", name); failures++; } }

static inline void server_remove_wait( unsigned int msgh_id, const int *objs, void **objs_shm,
                                       int alert_obj, void *alert_obj_shm, int count )
{
    int i;
    mach_msg_return_t mr;
    __thread static mach_register_message_t message;

    message.header.msgh_remote_port = server_port;
    message.header.msgh_bits = msgh_bits_send;
    message.header.msgh_id = msgh_id;

    for (i = 0; i < count; i++)
    {
        struct event *obj = (struct event *)objs_shm[i];

        int refs = __atomic_sub_fetch( &obj->multiple_waiters, 1, __ATOMIC_SEQ_CST);
        if (refs < 0)
            __atomic_store_n( &obj->multiple_waiters, 0, __ATOMIC_SEQ_CST);
        message.shm_idx[i] = objs[i];
    }

    if (alert_obj)
    {
        struct event *obj = (struct event *)alert_obj_shm;

        int refs = __atomic_sub_fetch( &obj->multiple_waiters, 1, __ATOMIC_SEQ_CST);
        if (refs < 0)
            __atomic_store_n( &obj->multiple_waiters, 0, __ATOMIC_SEQ_CST);
        message.shm_idx[count++] = alert_obj;
    }

    message.shm_idx[0] |= (1 << 29);

    message.header.msgh_size = sizeof(mach_msg_header_t) +
                               count * sizeof(unsigned int);

    mr = mach_msg2( (mach_msg_header_t *)&message, MACH_SEND_MSG, message.header.msgh_size,
                     0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, 0 );

    if (mr != MACH_MSG_SUCCESS)
        ERR("Failed to send server remove wait: %#x\n", mr);
}
static inline int check_shm_contention( void **objs_shm, void *alert_obj_shm, int count, int tid )
{
    int i, val;

    for (i = 0; i < count; i++)
    {
        val = __atomic_load_n((int *)objs_shm[i], __ATOMIC_SEQ_CST);
        if (((struct mutex *)objs_shm[i])->msync_type == MSYNC_MUTEX)
        {
            if (val == 0 || val == ~0 || val == tid) return 1;
        }
        else
        {
            if (val != 0)  return 1;
        }
    }

    if (alert_obj_shm)
    {
        val = __atomic_load_n((int *)alert_obj_shm, __ATOMIC_SEQ_CST);
        if (val != 0)  return 1;
    }

    return 0;
}
static NTSTATUS msync_wait_multiple( const int *objs, void **objs_shm, int alert_obj, void *alert_obj_shm,
                                     int count, ULONGLONG *end, int tid )
{
    int ret, val;
    int *addr = shm_tid_map + tid;
    ULONGLONG ns_timeleft = 0;
    mach_msg_return_t mr;
    unsigned int msgh_id;
    int total_count = count + (alert_obj ? 1 : 0);

    __atomic_store_n( addr, 2, __ATOMIC_RELEASE );
    msgh_id = (tid << 8) | total_count;
    mr = server_register_wait( msgh_id, objs, objs_shm, alert_obj, alert_obj_shm, count );

    if (mr != MACH_MSG_SUCCESS)
        return STATUS_PENDING;

    while (__atomic_load_n( addr, __ATOMIC_ACQUIRE ) == 2)
    {
        if (check_shm_contention( objs_shm, alert_obj_shm, count, tid ))
        {
            /* The server still gets the registration sent above and, if nothing it waits
             * for is signaled by then, keeps this thread on every object's list. Nothing
             * took it off again, so each lost race leaked list nodes in the wineserver
             * without bound (pool exhausted, then malloc per wait, gigabytes after hours
             * of a game that waits on several objects in a hot loop). Unregister like
             * every other exit does; this also drops the alert object's waiter count,
             * which the loop here used to leave raised. */
            server_remove_wait( msgh_id, objs, objs_shm, alert_obj, alert_obj_shm, count );
            return STATUS_PENDING;
        }
    }

    do
    {
        if (end)
        {
            ns_timeleft = update_timeout( *end ) * 100;
            if (!ns_timeleft)
            {
                server_remove_wait( msgh_id, objs, objs_shm, alert_obj, alert_obj_shm, count );
                return STATUS_TIMEOUT;
            }
        }
        ret = ulock_wait( UL_COMPARE_AND_WAIT_SHARED | ULF_NO_ERRNO, addr, 1, ns_timeleft );
        val = __atomic_load_n( addr, __ATOMIC_ACQUIRE );
        if (!val)
            break;
    } while (ret == -EINTR || ret == -EFAULT);

    server_remove_wait( msgh_id, objs, objs_shm, alert_obj, alert_obj_shm, count );

    if (ret == -ETIMEDOUT) return STATUS_TIMEOUT;

    return STATUS_SUCCESS;
}
static void client_cases(void) {
    for(int signal=0;signal<3;signal++) {
        struct event values[3]={{0}};
        void *shm[2]={&values[0],&values[1]}; int objs[2]={1,2};
        values[signal].value=1; removals=0;
        int result=msync_wait_multiple(objs,shm,3,&values[2],2,NULL,1);
        check(result==STATUS_PENDING,"early return status");
        check(removals==1,"server unregister sent");
        check(values[0].multiple_waiters==0 && values[1].multiple_waiters==0,"object counts balanced");
        check(values[2].multiple_waiters==0,"alert count balanced");
    }
}

static int registrations[4], woken;
static struct { int low; } objects[4];
static void *get_shm(unsigned int index) { return &objects[index]; }
static int check_bit(int bit, unsigned int *value) { return (*value>>bit)&1; }
static void unregister_wait(mach_register_message_t *message,int tid,int count) {
    for(int i=0;i<count;i++) if(registrations[message->shm_idx[i]]) registrations[message->shm_idx[i]]--;
}
static void add_tid(unsigned int index,int tid) { registrations[index]++; }
static void wake_tid(int tid) { woken++; }
static void server_case(int signal) {
    mach_register_message_t receive_message={0}; int count=3,tid=1,i,is_mutex,val;
    struct { int low; } *obj;
    memset(registrations,0,sizeof(registrations)); memset(objects,0,sizeof(objects)); woken=0;
    for(i=0;i<count;i++) receive_message.shm_idx[i]=i;
    objects[signal].low=1;
        for (i = 0; i < count; i++)
        {
            if (i == 0 && check_bit( 29, receive_message.shm_idx + i ))
            {
                unregister_wait( &receive_message, tid, count );
                break;
            }
            is_mutex = check_bit( 28, receive_message.shm_idx + i );
            obj = get_shm( receive_message.shm_idx[i] );
            val = __atomic_load_n( &obj->low, __ATOMIC_SEQ_CST );
            if ((is_mutex && (val == 0 || val == ~0 || val == tid)) || (!is_mutex && val != 0))
            {
                if (i > 0) unregister_wait( &receive_message, tid, i );  /* objects 0..i-1 were added above */
                wake_tid( tid );
                break;
            }
            add_tid( receive_message.shm_idx[i], tid );
            if (i == count - 1)
            {
                /* The client can stop spinning and safely start waiting now */
                __atomic_store_n( shm_tid_map + tid, 1, __ATOMIC_RELEASE );
            }
        }
    check(woken==1,"signaled waiter awakened");
    check(registrations[0]+registrations[1]+registrations[2]==0,"all prior registrations released");
}
int main(void) {
    client_cases();
    for(int i=0;i<3;i++) server_case(i);
    printf("failures=%d\n",failures); return failures!=0;
}
