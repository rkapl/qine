namespace Qnx {
    /* QNX Native locking ? What is the relationship to fcntl? */
    /* unlock region of a file */
    static constexpr int QLK_UNLCK = 0;
    /* lock a region of a file */
    static constexpr int QLK_LOCK = 1;
    /* non-blocking lock */
    static constexpr int QLK_NBLCK = 2;   
    /* lock for writing */
    static constexpr int QLK_RLCK = 3;
    /* non-blocking lock for writing */
    static constexpr int QLK_NBRLCK = 4;

    static constexpr int QF_SETLK = 6;       /*  Set record locking info */
    static constexpr int QF_SETLKW = 7;       /*  Set record locking info;    */
    static constexpr int QF_GETLK = 14;      /*  Get record locking info */


    static constexpr int QF_RDLCK = 1;
    static constexpr int QF_WRLCK = 2;
    static constexpr int QF_UNLCK = 3;
}