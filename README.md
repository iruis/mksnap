# mksnap

## btrfs 스냅샷 생성 프로그램

핵심이되는 함수, 구조체, 상수값은 btrfs 소스코드에서 가져왔습니다. 소스코드 사용에 문제가 최대한 발생하지않도록 GPL v2 라이선스를 따릅니다.

참고: https://git.kernel.org/pub/scm/linux/kernel/git/kdave/btrfs-progs.git

사용 예: ./mksnap &lt;source&gt; [&lt;destination&gt;] &lt;count&gt;  
마운트 된 source 경로를 destination에 KST-yyyy.MM.dd-HH.mm.ss 형식의 스냅샷을 생성한다. destination을 생략 할 경우 source 폴더에 .snapshots 폴더를 생성하여 스냅샷을 만든다. 이때 count 개수를 넘게되면 이름순으로 정렬하여 첫번째부터 스냅샷을 제거한다.

crontab으로 등록할때의 예: 0 0 * * * /opt/mksnap /home /opt/snapshots 100 >> /opt/mksnap.log 2>&1  
마운트 된 /home 폴더를 /opt/snapshots에 최대 100개를 매일 정각에 스냅샷을 생성하며 출력되는 메시지를 /opt/mksnap.log에 저장한다. 아래는 위 상황에서 마운트 한 예제.  
/dev/mapper/vg-home on /home type btrfs (rw,relatime,space_cache,subvolid=257,subvol=/home)  
/dev/mapper/vg-home on /opt/snapshots type btrfs (rw,relatime,space_cache,subvolid=1010,subvol=/snapshots)  
