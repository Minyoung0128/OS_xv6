#include "types.h"
#include "user.h"
#include "stat.h"
int main()
{
	printf(1,"Let's test getnice\n");
	printf(1,"%d\n",getnice(1));
	printf(1,"%d\n",getnice(2));
	printf(1,"%d\n",getnice(3));
	
	printf(1, " Let's change value\n");
	
	setnice(1,15);
	setnice(2, 25);
	setnice(3,30);

	printf(1, "Let's Look\n");
	ps(0);
	
	printf(1, "Let's Look by pid\n");
	printf(1,"pid : 1\n");
	ps(1);
	printf(1,"pid: 2\n");
	ps(2);	
	printf(1,"pid: 3\n");
	ps(3);	
	printf(1,"pid: 5\n");
	ps(5);	
	printf(1,"pid: 10\n");
	ps(10);	
	exit();	
}

