using System.Diagnostics;

namespace RadioDoge
{
    internal class SPVNode
    {
        private Process spvProcess;
        private readonly string commandString = "-c -b -d -p scan";
        private bool isRunning;
        private readonly bool runInOwnWindow;
        private ProcessStartInfo startInfo;

        public SPVNode(bool runInOwnWindow)
        {
            this.runInOwnWindow = runInOwnWindow;
            isRunning = false;
            SetupStartInfo();
        }

        public SPVNode(bool runInOwnWindow, string commandString)
        {
            this.runInOwnWindow = runInOwnWindow;
            this.commandString = commandString;
            isRunning = false;
            SetupStartInfo();
        }

        private void SetupStartInfo()
        {
            if (runInOwnWindow)
            {
                startInfo = new ProcessStartInfo
                {
                    FileName = "spvnode",
                    RedirectStandardInput = false,
                    RedirectStandardOutput = false,
                    UseShellExecute = true,
                    CreateNoWindow = false,
                    WindowStyle = ProcessWindowStyle.Normal,
                    Arguments = commandString
                };
            }
            else
            {
                startInfo = new ProcessStartInfo
                {
                    FileName = "spvnode",
                    RedirectStandardInput = true,
                    RedirectStandardOutput = false,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    Arguments = commandString
                };
            }
        }

        public bool Start()
        {
            if (!isRunning)
            {
                Console.WriteLine("Starting SPV Node...");
                spvProcess = new Process { StartInfo = startInfo };
                spvProcess.Start();
                isRunning = true;
                return true;
            }
            return false;
        }

        public bool ExitOnUserInput()
        {
            if (isRunning)
            {
                while (Console.ReadLine() != "exit")
                {
                    Console.WriteLine("Waiting for 'exit'...");
                    Thread.Sleep(1000);
                }
                return Stop();
            }
            return false;
        }

        public bool Stop()
        {
            if (isRunning)
            {
                Console.WriteLine("Stopping SPV Node");
                if (!runInOwnWindow)
                {
                    spvProcess.StandardInput.Close();
                }
                spvProcess.Kill(true);
                spvProcess.Close();
                isRunning = false;
                return true;
            }
            return false;
        }
    }
}
