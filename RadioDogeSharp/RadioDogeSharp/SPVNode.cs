using System.Diagnostics;

namespace RadioDoge
{
    internal class SPVNode
    {
        Process spvProcess;
        string commandString = "-c -b -d -p scan";
        bool isRunning;

        public SPVNode()
        {
            isRunning = false;
        }

        public SPVNode(string commandString)
        {
            this.commandString = commandString;
            isRunning = false;
        }

        public bool Start()
        {
            if (!isRunning)
            {
                ProcessStartInfo startInfo = new ProcessStartInfo
                {
                    FileName = "spvnode",
                    RedirectStandardInput = true,
                    RedirectStandardOutput = false,
                    UseShellExecute = false,
                    CreateNoWindow = false,
                    Arguments = commandString
                };
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
                spvProcess.StandardInput.Close();
                spvProcess.Kill(true);
                spvProcess.Close();
                isRunning = false;
                return true;
            }
            return false;
        }
    }
}
