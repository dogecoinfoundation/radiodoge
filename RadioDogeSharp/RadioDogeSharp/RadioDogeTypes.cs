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
        DisplayControl = 0x64, // Translates to sending 'd'
        HostFormedPacket = 0x68, // Translates to sending 'h'
        MultipartPacket = 0x6D, // Translates to sending 'm'
        ResultCode = 0xFE
    }

    internal enum DisplayType
    {
        Custom,
        RadioDogeLogo,
    }

    internal enum DogeCommandType
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

    internal enum TestFunctions
    {
        SendMultipartPacket,
        SendSinglePacket,
        SendCountingTest,
        DisplayTest,
    }
}
