/*
car drive problem solution implemented successfully.
To get the last final solution change the function lock_release_if_required to lock_release()

Final soltuion to whale problem.
*/

/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

/*struct semaphore * maleSemaphore;
struct semaphore * femaleSemaphore;
struct semaphore * matchmakerSemaphore;*/

int numberOfMales;
int numberOfFemales;
int numberOfMatchmakers_male;
int numberOfMatchmakers_female;

int tempMalecount;
int tempfemalecount;
int tempmmcount;

struct cv * cv_male;
struct cv * cv_female;
struct cv * cv_matchmaker;

struct lock * lock_male;
struct lock * lock_female;
struct lock * lock_matchmaker;

void whalemating_init()
{
	/*maleSemaphore = sem_create("maleSemaphore", 0);
	femaleSemaphore = sem_create("femaleSemaphore", 0);
	matchmakerSemaphore = sem_create("matchmakerSemaphore", 0);*/

	numberOfMales = 0;
	numberOfFemales = 0;
	numberOfMatchmakers_male = 0;
	numberOfMatchmakers_female = 0;

	cv_male = cv_create("cv_male");
	cv_female = cv_create("cv_female");
	cv_matchmaker = cv_create("cv_matchmaker");

	lock_male = lock_create("lock_male");
	lock_female = lock_create("lock_female");
	lock_matchmaker = lock_create("lock_matchmaker");

	 tempMalecount = 0;
	 tempfemalecount = 0;
	 tempmmcount =0;

  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
	/*sem_destroy(maleSemaphore);
	sem_destroy(femaleSemaphore);
	sem_destroy(matchmakerSemaphore);*/

  return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  
  male_start();
	// Implement this function 
  /*V(maleSemaphore);
  P(femaleSemaphore);
  P(matchmakerSemaphore);*/


  lock_acquire(lock_male);
  tempMalecount++;
  numberOfMales = numberOfMales+1;
  cv_broadcast(cv_male, lock_male);
  lock_release(lock_male);

  /*lock_acquire(lock_female);
  while(numberOfFemales == 0)
  {
	  cv_wait(cv_female, lock_female);
  }
  numberOfFemales = numberOfFemales-1;
  lock_release(lock_female);*/

  lock_acquire(lock_matchmaker);
  while(numberOfMatchmakers_male == 0)
  {
	  cv_wait(cv_matchmaker, lock_matchmaker);
  }
  numberOfMatchmakers_male = numberOfMatchmakers_male-1;
  lock_release(lock_matchmaker);

  male_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

/*void testing(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	while(true)
	{
		if(tempMalecount==10 && tempmmcount==10)
		{
			int k=0;
				for(k=0; k<10; k++)
				{
					int err = err = thread_fork("Female Whale Thread", female, whalematingMenuSemaphore, k, NULL);
					if (err) {
						panic("whalemating: thread_fork failed: (%s)\n", strerror(err));
					}
				}
				break;
		}
	}
}*/


void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  
  female_start();

	// Implement this function

    /*P(maleSemaphore);
    V(femaleSemaphore);
    P(matchmakerSemaphore);*/
  lock_acquire(lock_female);
  tempfemalecount++;
    numberOfFemales = numberOfFemales+1;
    cv_broadcast(cv_female, lock_female);
    lock_release(lock_female);

    lock_acquire(lock_matchmaker);
        while(numberOfMatchmakers_female == 0)
        {
      	  cv_wait(cv_matchmaker, lock_matchmaker);
        }
        numberOfMatchmakers_female = numberOfMatchmakers_female-1;
        lock_release(lock_matchmaker);

    /*lock_acquire(lock_male);
    while(numberOfMales == 0)
    {
  	  cv_wait(cv_male, lock_male);
    }
    numberOfMales = numberOfMales-1;
    lock_release(lock_male);*/


  female_end();
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;

  matchmaker_start();
	// Implement this function
  /*P(maleSemaphore);
  P(femaleSemaphore);
  V(matchmakerSemaphore);*/
  lock_acquire(lock_matchmaker);
  tempmmcount++;
  lock_release(lock_matchmaker);

    lock_acquire(lock_male);
            while(numberOfMales == 0)
            {
          	  cv_wait(cv_male, lock_male);
            }
            numberOfMales = numberOfMales-1;
            lock_release(lock_male);

        lock_acquire(lock_female);
              while(numberOfFemales == 0)
              {
            	  cv_wait(cv_female, lock_female);
              }
              numberOfFemales = numberOfFemales-1;
              lock_release(lock_female);


      lock_acquire(lock_matchmaker);
      numberOfMatchmakers_male = numberOfMatchmakers_male+1;
      numberOfMatchmakers_female = numberOfMatchmakers_female+1;
      cv_broadcast(cv_matchmaker, lock_matchmaker);
      lock_release(lock_matchmaker);

  matchmaker_end();
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct lock * lock0;
struct lock * lock1;
struct lock * lock2;
struct lock * lock3;

void stoplight_init()
{
	lock0 = lock_create("lock0");
	lock1 = lock_create("lock1");
	lock2 = lock_create("lock2");
	lock3 = lock_create("lock3");

  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
	lock_destroy(lock0);
	lock_destroy(lock1);
	lock_destroy(lock2);
	lock_destroy(lock3);
  return;
}

void
gostraight(void *p, unsigned long dir)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)dir;

  int direction = dir;
  struct lock * firstLock;
  struct lock * secondLock;
  int secondQuadrant;
  
  switch(direction)
    {
    case 0 :
    	firstLock = lock0;
    	secondLock = lock3;
    	secondQuadrant = 3;
  	  break;

    case 1 :
    	firstLock = lock1;
    	secondLock = lock0;
    	secondQuadrant = 0;
    	  break;

    case 2 :
    	firstLock = lock2;
    	secondLock = lock1;
    	secondQuadrant = 1;
    	  break;

    case 3 :
    	firstLock = lock3;
    	secondLock = lock2;
    	secondQuadrant = 2;
    	  break;

    default:
  	  panic("Not a valid direction\n");
  	  break;
    }


  while(true)
  {
	  lock_acquire(firstLock);

	  lock_acquire_no_sleep(secondLock);

	  if(lock_do_i_hold(firstLock) && lock_do_i_hold(secondLock))
		  break;
	  else
		  lock_release(firstLock);
  }

  inQuadrant(direction);
  inQuadrant(secondQuadrant);
  leaveIntersection();

  lock_release(firstLock);
  lock_release(secondLock);


  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long dir)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)dir;

  int direction = (int)dir;
  struct lock * firstLock;
  struct lock * secondLock;
  struct lock * thirdLock;
  int secondQuadrant;
  int thirdQuadrant;
  
  switch(direction)
      {
      case 0 :
      	firstLock = lock0;
      	secondLock = lock3;
      	secondQuadrant = 3;

      	thirdLock = lock2;
      	thirdQuadrant = 2;
    	  break;

      case 1 :
      	firstLock = lock1;
      	secondLock = lock0;
      	secondQuadrant = 0;

      	thirdLock = lock3;
      	thirdQuadrant = 3;
      	  break;

      case 2 :
      	firstLock = lock2;
      	secondLock = lock1;
      	secondQuadrant = 1;

      	thirdLock = lock0;
      	thirdQuadrant = 0;
      	  break;

      case 3 :
      	firstLock = lock3;
      	secondLock = lock2;
      	secondQuadrant = 2;

      	thirdLock = lock1;
      	thirdQuadrant = 1;
      	  break;

      default:
    	  panic("Not a valid direction\n");
    	  break;
      }

  	while(true)
    {
  		lock_acquire(firstLock);
  		lock_acquire_no_sleep(secondLock);


  	  if(lock_do_i_hold(firstLock) && lock_do_i_hold(secondLock))
  	  {
  		lock_acquire_no_sleep(thirdLock);
  		if(lock_do_i_hold(firstLock) && lock_do_i_hold(secondLock) && lock_do_i_hold(thirdLock))
  		{
  			break;
  		}
  		else
  		{
  			lock_release(firstLock);
  			lock_release(secondLock);
  		}
  	  }
  	  else
  	  {
  		lock_release(firstLock);
  	  }

    }

    inQuadrant(direction);
    inQuadrant(secondQuadrant);
    inQuadrant(thirdQuadrant);
    leaveIntersection();

    lock_release(firstLock);
    lock_release(secondLock);
    lock_release(thirdLock);

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long dir)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  //(void)direction;
  int direction = (int)dir;
  struct lock * lockForRight;

  switch(direction)
  {
  case 0 :
	  lockForRight = lock0;
	  break;

  case 1 :
  	  lockForRight = lock1;
  	  break;

  case 2 :
  	  lockForRight = lock2;
  	  break;

  case 3 :
  	  lockForRight = lock3;
  	  break;

  default:
	  panic("Not a valid direction\n");
	  break;
  }

  lock_acquire(lockForRight);
  inQuadrant(direction);
  leaveIntersection();
  lock_release(lockForRight);

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}
