// An Electron window whose native message box opens the way Code - OSS asks
// before opening a web page: on SIGUSR1, over this window, from Electron's own
// GTK dialog on Wayland. SIGUSR2 answers it.
const { app, BrowserWindow, dialog } = require('electron');

app.whenReady().then(() => {
    const win = new BrowserWindow({ width: 560, height: 420, title: 'Electron box probe', backgroundColor: '#2a6f97' });
    win.on('page-title-updated', (event) => event.preventDefault());
    win.loadURL('about:blank');
    let answer = null;
    process.on('SIGUSR1', () => {
        const box = new AbortController();
        answer = () => box.abort();
        dialog.showMessageBox(win, {
            type: 'question', buttons: ['Open', 'Cancel'], title: 'Code - OSS',
            message: 'Do you want Code - OSS to open the external website?', signal: box.signal,
        });
    });
    process.on('SIGUSR2', () => { if (answer) answer(); });
});
app.on('window-all-closed', () => app.quit());
