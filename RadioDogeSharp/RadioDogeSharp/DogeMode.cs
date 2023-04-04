namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private void DogeCommandLoop()
        {
            ConsoleHelper.PrintDogeCommandHelp();
        }

        private void RequestDogeCoinAddress(NodeAddress destNode)
        {
            // Create payload
            byte[] payload = new byte[] { (byte)DogeCommands.GetDogeAddress };
            SendPacket(destNode, payload);
        }

        private void RequestDogeCoinBalance(NodeAddress destNode, byte[] dogeCoinAddress)
        {
            List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
            payload.Add((byte)DogeCommands.GetBalance);
            payload.AddRange(dogeCoinAddress);
            SendPacket(destNode, payload.ToArray());
        }

        private void ProcessDogePayload(byte[] payload)
        {
            DogeCommands commType = (DogeCommands)payload[0];
            switch (commType)
            {
                case DogeCommands.SendDogeAddress:
                    // This means we got sent a dogecoin address
                    int addrLength = payload.Length - 1;
                    char[] dogecoinAddress = new char[addrLength];
                    Array.Copy(payload, 1, dogecoinAddress, 0, addrLength);
                    string addressString = new string(dogecoinAddress);
                    Console.WriteLine($"Received Dogecoin Address: {addressString}");
                    break;
                case DogeCommands.GetDogeAddress:
                    // This means someone is requesting a dogecoin address
                    // @TODO
                    break;
                default:
                    Console.WriteLine("Unknown payload. Raw data:");
                    PrintPayloadAsHex(payload);
                    break;
            }
        }
    }
}
