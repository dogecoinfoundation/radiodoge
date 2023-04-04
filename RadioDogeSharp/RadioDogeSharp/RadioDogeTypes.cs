namespace RadioDoge
{
    internal enum SerialCommandType
    {
        None,
        GetNodeAddress,
        SetNodeAddresses,
        Ping,
        Message,
        HardwareInfo = 0x3f, //Translates to sending '?'
        HostFormedPacket = 0x68, // Translates to sending 'h'
        MultipartPacket = 0x6D, // Translates to sending 'm'
        ResultCode = 0xFE
    }

    internal enum DogeCommands
    {
        GetDogeAddress = 120,
        SendDogeAddress = 240,
        GetBalance = 101,
        SendBalance = 202
    }

    internal enum ModeSelection
    {
        LoRaSetup,
        Doge,
        Test,
        Quit,
        Invalid
    }
}
