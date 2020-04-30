from celery import Celery
app = Celery()

@app.task
def run_command(cmd):
    cmd.write_to_snapshot_log(0)
    returncode = cmd.run()

if __name__ == '__main__':
    app.worker_main()