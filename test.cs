 var info = new ProcessStartInfo()
        {
            FileName = "Path\to\FontReg.exe",
            Arguments = "/copy",
            UseShellExecute = false,
            WindowStyle = ProcessWindowStyle.Hidden

        };

   Process.Start(info);