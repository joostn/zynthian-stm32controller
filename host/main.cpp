#include <stdio.h>
#include <stdlib.h>

#include "hidapi.h"
#include <vector>
#include <thread>
#include <iostream>
#include <exception>

#ifndef __APPLE__
#define WITHALSA 1
#else
#define WITHALSA 0
#endif

#if WITHALSA
extern "C" {
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
}
#endif

using namespace std::literals::chrono_literals;

static uint16_t s_UsbVendorId = 0xdead;
static uint16_t s_UsbProductId = 0xf001;

class TjnFinally
{
public:
  typedef std::function<void(void)> TFunc;
  inline TjnFinally(TFunc &&func)
    : m_Func(std::move(func)) {}
  inline ~TjnFinally()
  {
    if(!m_Dismissed)
    {
      m_Func();
    }
  }
  void Dismiss() noexcept(true)
  {
    m_Dismissed = true;
  }

private:
  TFunc m_Func;
  bool m_Dismissed = false;
};

#if WITHALSA
// Create a TjnAlsaMixer to control the volume:
//
// TjnAlsaMixer mixer(); // try to find default volume control
// or:
// TjnAlsaMixer mixer("control_name"); // use specific mixer control
//
// mixer.SetVolume(v);  // [0..4095]
class TjnAlsaMixer
{
private:
    snd_mixer_t *m_Handle = nullptr;
    snd_mixer_selem_id_t *m_Sid = nullptr;
	snd_mixer_elem_t* m_Elem = nullptr;

public:
	TjnAlsaMixer(const std::string &cardname = {})
	{
		try
		{
			{
				auto errcode = snd_mixer_open(&m_Handle, 0);
				if(errcode)
				{
					throw std::runtime_error("snd_mixer_open failed");
				}
			}
			{
				const char *card = "default";
				auto errcode = snd_mixer_attach(m_Handle, card);
				if(errcode)
				{
					throw std::runtime_error("snd_mixer_attach failed");
				}
			}
			{
				auto errcode = snd_mixer_selem_register(m_Handle, NULL, NULL);
				if(errcode)
				{
					throw std::runtime_error("snd_mixer_selem_register failed");
				}
			}
			{
				auto errcode = snd_mixer_load(m_Handle);
				if(errcode)
				{
					throw std::runtime_error("snd_mixer_load failed");
				}
			}

			if(!cardname.empty())
			{
				{
					snd_mixer_selem_id_alloca(&m_Sid);
					if(!m_Sid)
					{
						throw std::runtime_error("snd_mixer_selem_id_alloca failed");
					}
				}
				snd_mixer_selem_id_set_index(m_Sid, 0);
				snd_mixer_selem_id_set_name(m_Sid, cardname.c_str());
				{		
					m_Elem = snd_mixer_find_selem(m_Handle, m_Sid);
					if(!m_Elem)
					{
						auto errtxt = std::string("Could not find mixer control: ")+cardname;
						throw std::runtime_error(errtxt);				
					}
				}
			}
			else
			{
				// find the first elem with playback volume control:
				snd_mixer_elem_t *elem = nullptr;
				for (elem = snd_mixer_first_elem(m_Handle); elem; elem = snd_mixer_elem_next(elem))
				{
					if(snd_mixer_selem_is_active(elem))
					{
						if (snd_mixer_selem_has_playback_volume(elem))
						{
							if(m_Elem)
							{
								throw std::runtime_error("Multiple mixer elements found for controlling playback volume. Please specify which volume control to use. Run amixer -M to get a list.");
							}
							m_Elem = elem;
						}
					}
				}
				if(!m_Elem)
				{
					throw std::runtime_error("No mixer element found for controlling volume");
				}
				std::string elemname;
				{
					snd_mixer_selem_id_t *sid;
					snd_mixer_selem_id_alloca(&sid);
					snd_mixer_selem_get_id(m_Elem, sid);
					elemname = snd_mixer_selem_id_get_name(sid);
				}
				std::cout << "Using mixer element: " << elemname << "\n";
			}
		}
		catch(...)
		{
			Free();
			throw;
		}
	}
	~TjnAlsaMixer()
	{
		Free();
	}
	void Free()
	{
		if(m_Handle)
		{
				snd_mixer_close(m_Handle);
				m_Handle = nullptr;
		}
	}
	void SetVolume(int val_12bit)
	{
		// val ranges from [0..4095]
		long min, max;
		{
			auto errcode = snd_mixer_selem_get_playback_volume_range(m_Elem, &min, &max);
			if(errcode)
			{
				throw std::runtime_error("snd_mixer_selem_get_playback_volume_range failed");
			}
		}
		long scaledvolume = min + ((max - min) * val_12bit) / 4095;
		{
		    auto errcode = snd_mixer_selem_set_playback_volume_all(m_Elem, scaledvolume);
			if(errcode)
			{
				throw std::runtime_error("snd_mixer_selem_set_playback_volume_all failed");
			}
		}
	}

};
#endif

void logerror(std::exception_ptr e)
{
	if(e)
	{
		try
		{
			std::rethrow_exception(e);
		}
		catch(std::exception &e)
		{
			std::cout << e.what() << "\n";
		}
	}
}

int main(int argc, char* argv[])
{
	bool wasConnected = true;
	std::string elemname;
	if (argc >= 2) 
	{
		elemname = argv[1];
    }
	try
	{
	#if WITHALSA
		TjnAlsaMixer mixer(elemname);
	#endif
		while(true)
		{
			// Try to open the HID device:
			auto handle = hid_open(s_UsbVendorId, s_UsbProductId, NULL);
			if(handle)
			{
				wasConnected = true;
				TjnFinally fin1([&](){
					hid_close(handle);
					handle = nullptr;
				});
				std::wcout << L"Connected!\n";
				try
				{
					int counter = 0;
					while(true)
					{
						// Read data (this blocks until the device sends something):
						std::vector<unsigned char> buf(65);
						int res = hid_read(handle, buf.data(), buf.size());
						if (res < 0)
						{
							throw std::runtime_error("read failed");
						}
						if(res < 4)
						{
							throw std::runtime_error("expecting at least 4 bytes");
						}
						// device version; we can used this in the future to distinguish revisions:
						[[maybe_unused]] int version = ((int)buf[0]) | (((int)buf[1]) << 8);
						int volume = ((int)buf[2]) | (((int)buf[3]) << 8);
#if WITHALSA
						mixer.SetVolume(volume);
#else
						std::wcout << L"Volume: " << volume << "\n";
#endif
					}
				}
				catch(...)
				{
					logerror(std::current_exception());
				}
			}
			else
			{
				if(wasConnected)
				{
					std::wcout << L"Device not found, sleeping...\n";
				}
				std::this_thread::sleep_for(1000ms);
				wasConnected = false;
			}
		}
	}
	catch(...)
	{
		logerror(std::current_exception());
		return 1;
	}
	return 0;
}
