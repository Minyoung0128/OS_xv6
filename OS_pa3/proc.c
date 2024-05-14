#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

#define MMAPBASE 0x40000000
struct mmap_area mma[64] = {0};

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // 시작할 때 mma의 valid bit -1로 초기화
  for(int i=0; i<64; i++){
      mma[i].valid = -1;
    }

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  // 새로운 프로세스의 메모리 영역 만들어줘야 함.
  // 부모와 똑같은 메모리 영역을 가져야 한다. 
  
  // mma 뒤져서 부모가 가지고 있는 memory mapping 복사 
  struct mmap_area* m;
  for(i=0;i<64;i++){
    m = &mma[i];
    struct mmap_area* copy_m ;
    if(m->p == curproc && m->valid != -1){
      // 복사해오기 
      for(int j=0;j<64;j++){
        // 빈 영역 찾기 
        copy_m = &mma[j];
        
        if(copy_m->valid != -1) continue;
      
        copy_m->f = m->f;
        copy_m->addr = m->addr;
        copy_m->length = m->length;
        copy_m->offset = m->offset;
        copy_m->prot = m->prot;
        copy_m->flags = m->flags;
        copy_m->p = np;

        if(m->valid == 1){
          // 실제 physical memory가 할당되어 있으니까 가져와줘야돼
          
          if((m->flags & MAP_ANONYMOUS)){
            // 0으로 채워주기 
            for(int k=0;k<copy_m->length;k+=PGSIZE){
              char* m_area = kalloc();
              
              if(m_area == 0) return 0; // kalloc 실패 
              memset(m_area, 0, PGSIZE);
            }

            copy_m->valid = 1;
          }
          else{
            // cprintf("Let'f file read\n\n");
            // file 가져와야함
            // copy_m->f->off = copy_m -> offset;

            for(int k=0;k<copy_m->length;k+=PGSIZE){
              char* m_area = kalloc();
              
              if(m_area == 0) return 0; // kalloc 실패 
              memset(m_area, 0, PGSIZE);

              filedup(copy_m->f);
              fileread(copy_m->f, m_area, PGSIZE);

              pde_t* pde_idx = np->pgdir;
              if(mappages(pde_idx, (void*)(m->addr + k), PGSIZE, V2P(m_area), m->prot|PTE_U)==-1) return 0;

              copy_m->valid = 1;

            }
            
          }

          break;
        }
        else{
          // 안가져와도 ㄱㅊ 
              copy_m -> valid = 0;
          }
            
        break;
      }
    }
  }

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

struct mmap_area*
find_mmap_area(uint addr){
  struct mmap_area* m = 0;
  struct proc* p = myproc();
  int i = 0;

  for(i = 0;i<64;i++){
    m = &mma[i];
    if(m->addr <= addr && (m->addr + m->length) >= addr && m->p == p && m->valid != -1) {
      // cprintf("M addr : %d\n Find : %d\n",m->addr, addr);
      return m;
      }
  }
  // cprintf("Found Fail %d\n",m);
  return 0;
}

uint mmap(uint addr, int length, int prot, int flags, int fd, int offset){
  
  struct proc* p = myproc();
  
  uint start_address = addr + MMAPBASE;

  // Break point check 

  if(start_address % PGSIZE != 0){
    // cprintf("Not aligned Page Address\n");
    return 0;
  }
  struct file *f = 0;

  if((fd < 0 && fd!= -1) || fd >= 16){
    // file descriptor 숫자는 16으로 제한
    return 0;
  }

  if(fd!=-1){
    f = p->ofile[fd];
    if(f==0) return 0; // file 유효하지 않음
  }

  if((flags & MAP_ANONYMOUS) == 1 && (fd!=-1 || offset!=0)) {
    // Anonymous이면 fd가 -1, offset이 0이어야함
    return 0;
  }

  if((flags & MAP_ANONYMOUS) == 0){
    // file을 읽어야와야 함
    if((prot & PROT_READ) && !(f->readable)){
      // 읽을라했는데 읽을 수 없는 파일
      return 0;
    }
    if((prot & PROT_WRITE) && !(f->writable)){
      return 0;
    }
  }

  // mmap 시작

  // cprintf("Start MMAP\n"); 
  struct mmap_area *tmp = &mma[0];
  char* mem_area = 0; //  여기에다가 메모리 할당

  int idx = 0;

  for(idx = 0;idx<64;idx++){
    tmp = &mma[idx];
    if(tmp->valid==-1){
      break;
    } 
  }

  if(idx>=64){
    return 0;
  }

  // 여기로 왔으면, tmp가 빈 mma를 가리키고 있음

  // cprintf("Write in %dth idx\n",idx);

  tmp->p = p;
  tmp->addr = start_address;
  tmp->offset = offset;
  tmp->f = f;
  tmp->flags = flags;
  tmp->length = length;
  tmp->prot = prot;
  
  pde_t *pde_idx = p->pgdir;

  // populate 체크
  // populate가 아니면 지금 page table 업데이트 안해줘도 됨
  // 나중에 page fault 발생 시 그때 populate 수행
  if (flags == 0 || flags == 1){
    // not populated 
    tmp->valid = 0;
    // cprintf("Not Populated mapping\n");
    return start_address;
  }

  else if(flags == 2){
    // Not Anonymous + populated
    // file 읽어오고, page 업데이트 둘 다 해줘야 함. 
    filedup(f);

    for(int i =0;i<length;i+=PGSIZE){
      mem_area = kalloc();
      if(mem_area==0) return 0;

      // Memory 0으로 비워주기 
      memset(mem_area,0,PGSIZE);

      // physical address에 저장을 해주고 그걸 virtual이랑 연결
      fileread(f, mem_area, PGSIZE);
      
      // mappage의 perm 인자가 뭘 말하는지 모르겠다
      // 뭔가.. 권한 설정인 것 같음 
      // PTE_U > usermode에서 읽을 수 있는 page table Entry
      // prot으로 그 page를 read, write 할 수 있는지 권한 관리 + usermode에서도 사용할 수 있도록 OR 연산 수행
      if(mappages(pde_idx, (void*)(start_address + i), PGSIZE, V2P(mem_area), prot|PTE_U)==-1) return 0;

      tmp->valid = 1;
    }

  }
  else if(flags == 3){
    // Anonymous + populated
    // 메모리 영역 0으로 채우기
    for(int i =0;i<length;i+=PGSIZE){
      mem_area = kalloc();
      if(mem_area==0) return 0;

      memset(mem_area,0,PGSIZE);

      if(mappages(pde_idx, (void*)(start_address + i), PGSIZE, V2P(mem_area), prot|PTE_U)==-1) return 0;

      tmp->valid = 1;
    }
  }
  
  // cprintf("Memory map start %d\n",tmp->valid);

  return start_address;

}

int page_fault_handler(uint addr, uint err){
  // 연결이 안되어있는 addr을 받아옴 -> mma에서 해당 addr이 있는지 찾고 있으면 그때 읽어오기
  // cprintf("PageFaultHandle\n");
  struct mmap_area* m;
  char *mem_area = 0;

  m = find_mmap_area(addr);

  if(m==0) return -1;

  if((err&2) ==0){
    // Read Error니까 해당 page가 읽을수있는지 체크 
    if(((m->prot) & PROT_READ) == 0) return -1;
  }

  else if(err&2){
    // write Error
    if(((m->prot)& PROT_WRITE)== 0) {
      return -1;
      }
  }

  // mem area valid 체크
  if(m->valid == -1){
    // 할당되지 않은 mma area 참조
    // cprintf("Not Allocated Area\n");
    return -1;
  }

  if(m->valid == 1){
    // page already exist
    // cprintf("Page already exist\n");
    return -1;
  }

  if(m->valid == 0){
    // 읽기 시작
    // cprintf("Valid is 0\n");

    pde_t *pde_idx = m->p->pgdir;
    // cprintf("pde idx %d\n",pde_idx);
    if(m -> flags == 0){
      // Not Anonymous > should read file
      struct file* f = m->f;

      if(f==0){
        return -1;
      }

      if((m->prot & PROT_READ) && !(f->readable)){
      // 읽을라했는데 읽을 수 없는 파일
        return -1;
      }
      if((m->prot & PROT_WRITE) && !(f->writable)){
        return -1;
      }

      filedup(f);

      f->off = m->offset;

      for(int i =0; i < m->length; i += PGSIZE){

        mem_area = kalloc();

        if(mem_area==0) return 0;

        memset(mem_area ,0,PGSIZE);
        fileread(f, mem_area, PGSIZE);

        if(mappages(pde_idx, (void*)(m->addr + i), PGSIZE, V2P(mem_area), m->prot|PTE_U|PTE_W)==-1) return -1;
        }
    }

    else if(m->flags == 1){
      // Anonymous + not populated
      // cprintf("Anonymous + not populated\n");
      for (int i = 0 ;i < m->length; i+= PGSIZE){
        mem_area = kalloc();

        if(mem_area==0) return 0;

        memset(mem_area,0,PGSIZE);

        if(mappages(pde_idx, (void*)(m->addr+ i), PGSIZE, V2P(mem_area),m->prot|PTE_U|PTE_W)==-1) return -1;

      }
    }

    else{
      // flags가 2, 3인데 여기에 들어오는 거면 swap이 되었다는 거. 
      // 다음 과제에서 처리할듯?
      return -1;
    }

    // cprintf("Page fault handler Complete\n");
    m->valid = 1; 

    return 0;
  }
  
  return -1;

}

int munmap(uint addr){
  
  // page align 확인
  if(addr % PGSIZE != 0){
    // cprintf("Not aligned Address\n");
    return 0;
  }

  struct mmap_area* m;
  m = find_mmap_area(addr);

  if(m == 0) {
    // cprintf("addr %d is not exist!!!!\n",addr);
    return -1;
  } // 해당 address가 mmap area에 존재하지 않음

  pde_t* pde_idx = m->p->pgdir;
  pte_t* pte;
  
  if(m->valid == 0){
    // page가 아직 allocate 되지 않은 mmap area
    m->valid = -1;
    return 1;
  }

  if(m->valid == 1){
    for(int i = 0;i<m->length;i+=PGSIZE){
      pte = walkpgdir(pde_idx, (char *)(addr + i), 0);
      if(pte == 0) return -1;

      uint pte_flag = PTE_FLAGS(*pte);

      if((pte_flag & PTE_P)==0) {
        // cprintf("Here\n");
        continue;
      } // 이미 pte가 초기화된 상황이니까 굳이 안해줘도 됨

      // physical address가져오기
      uint p_addr = PTE_ADDR(*pte);

      kfree(P2V(p_addr));
      *pte = 0;

    }

    m->valid = -1;
    lcr3(V2P(myproc()->pgdir));
    return 1;
  }
  
  return 0;
}

  

uint freemem(){
  return freemem_count();
}