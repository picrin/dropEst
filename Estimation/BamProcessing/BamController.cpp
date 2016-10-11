#include "BamController.h"

#include "Tools/Logs.h"
#include "Tools/ReadParameters.h"
#include "FilledBamParamsParser.h"
#include "ReadMapParamsParser.h"

#include <api/BamReader.h>

namespace Estimation
{
namespace BamProcessing
{
	const std::string BamController::GENE_TAG = "GE";
	const std::string BamController::CB_TAG = "CB";
	const std::string BamController::UMI_TAG = "UB";

	void BamController::parse_bam_files(const std::vector<std::string> &bam_files, bool print_result_bams,
										bool filled_bam, const std::string &reads_params_names_str,
										const std::string &gtf_path, CellsDataContainer &container)
	{
		Tools::trace_time("Start parse bams");

		std::shared_ptr<BamProcessor> processor = std::make_shared<BamProcessor>(container, print_result_bams);
		BamController::parse_bam_files(bam_files, print_result_bams, filled_bam, reads_params_names_str, gtf_path, processor);

		Tools::trace_time("Bams parsed");
	}

	void BamController::parse_bam_file(const std::string &bam_name, std::shared_ptr<BamProcessor> &processor,
									   std::shared_ptr<ReadsParamsParser> &parser)
	{
		using namespace BamTools;

		BamReader reader;
		if (!reader.Open(bam_name))
			throw std::runtime_error("Could not open BAM file: " + bam_name);

		processor->update_bam(bam_name, reader);

		BamAlignment alignment;
		std::unordered_set<std::string> unexpected_chromosomes;

		while (reader.GetNextAlignment(alignment))
		{
			processor->inc_reads();

			std::string chr_name = reader.GetReferenceData()[alignment.RefID].RefName;

			Tools::ReadParameters read_params;
			const std::string &read_name = alignment.Name;
			if (!parser->get_read_params(alignment, read_params))
				continue;

			std::string gene;
			try
			{
				gene = parser->get_gene(chr_name, alignment);
			}
			catch (Tools::RefGenesContainer::ChrNotFoundException ex)
			{
				if (unexpected_chromosomes.emplace(ex.chr_name).second)
				{
					L_WARN << "WARNING: Can't find chromosome '" << ex.chr_name << "'";
				}
				continue;
			}

			processor->write_alignment(alignment, gene, read_params);
			processor->save_read(read_params.cell_barcode(), chr_name, read_params.umi_barcode(), gene);
		}

		reader.Close();
	}

	std::shared_ptr<ReadsParamsParser> BamController::get_parser(bool filled_bam, bool save_read_names,
																 const std::string &reads_params_names_str,
																 const std::string &gtf_path)
	{
		if (filled_bam)
			return std::make_shared<FilledBamParamsParser>(gtf_path);

		if (reads_params_names_str != "")
			return std::make_shared<ReadMapParamsParser>(gtf_path, save_read_names, reads_params_names_str);

		return std::make_shared<ReadsParamsParser>(gtf_path);
	}

	void BamController::parse_bam_files(const std::vector<std::string> &bam_files, bool print_result_bams, bool filled_bam,
								   const std::string &reads_params_names_str, const std::string &gtf_path,
								   std::shared_ptr<BamProcessor> processor)
	{
		std::shared_ptr<ReadsParamsParser> parser = BamController::get_parser(filled_bam, print_result_bams,
																			  reads_params_names_str, gtf_path);

		for (size_t i = 0; i < bam_files.size(); ++i)
		{
			BamController::parse_bam_file(bam_files[i], processor, parser);
			processor->trace_state(bam_files[i]);
		}
	}
}
}
