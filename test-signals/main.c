#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*
Durante l’esecuzione dell’handler possono arrivare altri segnali: questo può generare
situazioni inconsistenti.
- L’handler deve essere breve, semplicemente aggiornare lo stato interno e/o terminare
  l’applicazione.
- Non tutte le funzioni di libreria possono essere chiamate nell’handler con la garanzia
  che non succeda niente di strano (sul libro di testo p 616 o in rete cercate: 'asynchronous
  signal safe functions', trovate una lista). in particolare non è safe chiamare tipiche
  funzioni della libreria standard, come la printf(), la scanf() o altre funzioni definite
  all’interno del programma.

Quindi: usare i segnali il meno possibile, essenzialmente solo per situazioni standard:
- gestire SIGINT, SIGTERM e simili, per ripulire l’ambiente in caso si richieda la terminazione
  dell’applicazione.
- gestire SIGSEGV e simili, per evitare il display diretto di errori brutti tipo Segmentation
  fault, Bus error etc … (dopo segnali di questo tipo bisogna però SEMPRE uscire, la memoria
  non è più garantita essere consistente).
- ignorare SIGPIPE (ad esempio in modo da non far terminare il server se un client ha
  riattaccato).


*/

typedef struct sigaction sig_act_t;

void basic_signal_handler(int sig) {
    printf("Received signal code %d\n", sig);
    // exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    // Signal Action struct
    sig_act_t s;
    memset(&s, 0, sizeof(sig_act_t));
    s.sa_handler = basic_signal_handler;

    int signals[7];
    signals[0] = SIGINT;
    signals[1] = SIGTSTP;
    signals[2] = SIGQUIT;
    signals[3] = SIGHUP;
    signals[4] = SIGTERM;
    signals[5] = SIGSEGV;
    signals[6] = SIGPIPE;
    
    // Set action for signals
    for (int i = 0; i < 7; ++i) {
        if (sigaction(signals[i], &s, NULL) == -1) {
            perror("ERROR signaction");
            exit(EXIT_FAILURE);
        }
        printf("Registered handler for signal code %d at index %d\n", signals[i], i);
    }
    
    // 
    // source: https://en.wikipedia.org/wiki/Signal_(IPC)#POSIX_signals
    // 

    /* 
     * The SIGINT signal is sent to a process by its controlling terminal when a user wishes
     * to interrupt the process. This is typically initiated by pressing Ctrl+C, but on some
     * systems, the "delete" character or "break" key can be used.
     */
    raise(SIGINT);

    /* 
     * The SIGTSTP signal is sent to a process by its controlling terminal to request it to
     * stop (terminal stop). It is commonly initiated by the user pressing Ctrl+Z. Unlike
     * SIGSTOP, the process can register a signal handler for, or ignore, the signal.
     */
    raise(SIGTSTP);
    
    /*
     * The SIGQUIT signal is sent to a process by its controlling terminal when the user
     * requests that the process quit and perform a core dump.
     */
    raise(SIGQUIT);
    
    /*
     * The SIGHUP signal is sent to a process when its controlling terminal is closed. It was
     * originally designed to notify the process of a serial line drop (a hangup). In modern
     * systems, this signal usually means that the controlling pseudo or virtual terminal has
     * been closed. Many daemons (who have no controlling terminal) interpret receipt of this
     * signal as a request to reload their configuration files and flush/reopen their logfiles
     * instead of exiting. nohup is a command to make a command ignore the signal.
     */
    raise(SIGHUP);
    
    /* 
     * The SIGTERM signal is sent to a process to request its termination. Unlike the SIGKILL
     * signal, it can be caught and interpreted or ignored by the process. This allows the
     * process to perform nice termination releasing resources and saving state if appropriate.
     * SIGINT is nearly identical to SIGTERM.
     */
    raise(SIGTERM);
    
    /*
     * The SIGSEGV signal is sent to a process when it makes an invalid virtual memory
     * reference, or segmentation fault, i.e. when it performs a segmentation violation.
     */
    raise(SIGSEGV);
    
    /*
     * The SIGPIPE signal is sent to a process when it attempts to write to a pipe without a
     * process connected to the other end.
     */
    raise(SIGPIPE);
    
    return EXIT_SUCCESS;
}